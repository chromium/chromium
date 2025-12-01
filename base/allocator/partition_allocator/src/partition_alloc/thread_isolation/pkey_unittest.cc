// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if PA_BUILDFLAG(ENABLE_PKEYS)

#include <link.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "partition_alloc/address_space_stats.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/thread_isolation/pkey.h"
#include "testing/gtest/include/gtest/gtest.h"

#define ISOLATED_FUNCTION extern "C" __attribute__((used))
constexpr size_t kIsolatedThreadStackSize = 64 * 1024;
constexpr int kNumPkey = 16;
constexpr size_t kTestReturnValue = 0x8765432187654321llu;
constexpr uint32_t kPKRUAllowAccessNoWrite = 0b10101010101010101010101010101000;

namespace partition_alloc::internal {

struct PA_THREAD_ISOLATED_ALIGN IsolatedGlobals {
  int pkey = kInvalidPkey;
  void* stack;
  partition_alloc::internal::base::NoDestructor<
      partition_alloc::PartitionAllocator>
      allocator{};
} isolated_globals;

int ProtFromSegmentFlags(ElfW(Word) flags) {
  int prot = 0;
  if (flags & PF_R) {
    prot |= PROT_READ;
  }
  if (flags & PF_W) {
    prot |= PROT_WRITE;
  }
  if (flags & PF_X) {
    prot |= PROT_EXEC;
  }
  return prot;
}

int ProtectROSegments(struct dl_phdr_info* info, size_t info_size, void* data) {
  if (!strcmp(info->dlpi_name, "linux-vdso.so.1")) {
    return 0;
  }
  for (int i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
    if (phdr->p_type != PT_LOAD && phdr->p_type != PT_GNU_RELRO) {
      continue;
    }
    if (phdr->p_flags & PF_W) {
      continue;
    }
    uintptr_t start = info->dlpi_addr + phdr->p_vaddr;
    uintptr_t end = start + phdr->p_memsz;
    uintptr_t start_page = RoundDownToSystemPage(start);
    uintptr_t end_page = RoundUpToSystemPage(end);
    uintptr_t size = end_page - start_page;
    PA_PCHECK(PkeyMprotect(reinterpret_cast<void*>(start_page), size,
                           ProtFromSegmentFlags(phdr->p_flags),
                           isolated_globals.pkey) == 0);
  }
  return 0;
}

class PkeyTest : public testing::Test {
 protected:
  static void PkeyProtectMemory() {
    PA_PCHECK(dl_iterate_phdr(ProtectROSegments, nullptr) == 0);

    PA_PCHECK(PkeyMprotect(&isolated_globals, sizeof(isolated_globals),
                           PROT_READ | PROT_WRITE, isolated_globals.pkey) == 0);

    PA_PCHECK(PkeyMprotect(isolated_globals.stack, kIsolatedThreadStackSize,
                           PROT_READ | PROT_WRITE, isolated_globals.pkey) == 0);
  }

  static void InitializeIsolatedThread() {
    isolated_globals.stack =
        mmap(nullptr, kIsolatedThreadStackSize, PROT_READ | PROT_WRITE,
             MAP_ANONYMOUS | MAP_PRIVATE | MAP_STACK, -1, 0);
    PA_PCHECK(isolated_globals.stack != MAP_FAILED);

    PkeyProtectMemory();
  }

  void SetUp() override {
    // SetUp only once, but we can't do it in SetUpTestSuite since that runs
    // before other PartitionAlloc initialization happened.
    if (isolated_globals.pkey != kInvalidPkey) {
      return;
    }

    int pkey = PkeyAlloc(0);
    if (pkey == -1) {
      return;
    }
    isolated_globals.pkey = pkey;

    isolated_globals.allocator->init([] {
      partition_alloc::PartitionOptions opts;
      opts.thread_isolation = ThreadIsolationOption(isolated_globals.pkey);
      return opts;
    }());

    InitializeIsolatedThread();

    Wrpkru(kPKRUAllowAccessNoWrite);
  }

  static void TearDownTestSuite() {
    if (isolated_globals.pkey == kInvalidPkey) {
      return;
    }
    PA_PCHECK(PkeyMprotect(&isolated_globals, sizeof(isolated_globals),
                           PROT_READ | PROT_WRITE, kDefaultPkey) == 0);
    isolated_globals.pkey = kDefaultPkey;
    InitializeIsolatedThread();
    PkeyFree(isolated_globals.pkey);
  }
};

// This code will run with access limited to pkey 1, no default pkey access.
// Note that we're stricter than required for debugging purposes.
// In the final use, we'll likely allow at least read access to the default
// pkey.
ISOLATED_FUNCTION uint64_t IsolatedAllocFree(void* arg) {
  char* buf = (char*)isolated_globals.allocator->root()
                  ->Alloc<partition_alloc::AllocFlags::kNoHooks>(1024);
  if (!buf) {
    return 0xffffffffffffffffllu;
  }
  isolated_globals.allocator->root()->Free<FreeFlags::kNoHooks>(buf);

  return kTestReturnValue;
}

// This test is a bit compliated. We want to ensure that the code
// allocating/freeing from the pkey pool doesn't *unexpectedly* access memory
// tagged with the default pkey (pkey 0). This could be a security issue since
// in our CFI threat model that memory might be attacker controlled.
// To test for this, we run alloc/free without access to the default pkey. In
// order to do this, we need to tag all global read-only memory with our pkey as
// well as switch to a pkey-tagged stack.
TEST_F(PkeyTest, AllocWithoutDefaultPkey) {
  if (isolated_globals.pkey == kInvalidPkey) {
    return;
  }

  uint64_t ret;
  uint32_t pkru_value = 0;
  for (int pkey = 0; pkey < kNumPkey; pkey++) {
    if (pkey != isolated_globals.pkey) {
      pkru_value |= (PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE) << (2 * pkey);
    }
  }

  // Switch to the safe stack with inline assembly.
  //
  // The simple solution would be to use one asm statement as a prologue to
  // switch to the protected stack and a second one to switch it back. However,
  // that doesn't work since inline assembly doesn't support a clobbered stack
  // register. So instead, we switch the stack, perform a function call
  // to the
  // actual code and switch back afterwards.
  //
  // The inline asm docs mention that special care must be taken
  // when calling a function in inline assembly. I.e. we will
  // need to make sure that we follow the ABI of the platform.
  // In this example, we use the System-V ABI.
  //
  // == Caller-saved registers ==
  // We had two ideas for handling caller-saved registers. Option 1 was chosen,
  // but I'll describe both to show why option 2 didn't work out:
  // * Option 1) mark all caller-saved registers as clobbered. This should be
  //             in line with how the compiler would create the function call.
  //             Problem: future additions to caller-saved registers can break
  //             this.
  // * Option 2) use attribute no_caller_saved_registers. This prohibits use of
  //             sse/mmx/x87. We can disable sse/mmx with a "target" attribute,
  //             but I couldn't find a way to disable x87.
  //             The docs tell you to use -mgeneral-regs-only. Maybe we
  //             could move the isolated code to a separate file and then
  //             use that flag for compiling that file only.
  //             !!! This doesn't work: the inner function can call out to code
  //             that uses caller-saved registers and won't save
  //             them itself.
  //
  // == stack alignment ==
  // The ABI requires us to have a 16 byte aligned rsp on function
  // entry. We push one qword onto the stack so we need to subtract
  // an additional 8 bytes from the stack pointer.
  //
  // == additional clobbering ==
  // As described above, we need to clobber everything besides
  // callee-saved registers. The ABI requires all x87 registers to
  // be set to empty on fn entry / return,
  // so we should tell the compiler that this is the case. As I understand the
  // docs, this is done by marking them as clobbered. Worst case, we'll notice
  // any issues quickly and can fix them if it turned out to be false>
  //
  // == direction flag ==
  // Theoretically, the DF flag could be set to 1 at asm entry. If this
  // leads to problems, we might have to zero it before the fn call and
  // restore it afterwards. I would'ave assumed that marking flags as
  // clobbered would require the compiler to reset the DF before the next fn
  // call, but that doesn't seem to be the case.
  asm volatile(
      // Set pkru to only allow access to pkey 1 memory.
      ".byte 0x0f,0x01,0xef\n"  // wrpkru

      // Move to the isolated stack and store the old value
      "xchg %4, %%rsp\n"
      "push %4\n"
      "call IsolatedAllocFree\n"
      // We need rax below, so move the return value to the stack
      "push %%rax\n"

      // Set pkru to only allow access to pkey 0 memory.
      "mov $0b10101010101010101010101010101000, %%rax\n"
      "xor %%rcx, %%rcx\n"
      "xor %%rdx, %%rdx\n"
      ".byte 0x0f,0x01,0xef\n"  // wrpkru

      // Pop the return value
      "pop %0\n"
      // Restore the original stack
      "pop %%rsp\n"

      : "=r"(ret)
      : "a"(pkru_value), "c"(0), "d"(0),
        "r"(reinterpret_cast<uintptr_t>(isolated_globals.stack) +
            kIsolatedThreadStackSize - 8)
      : "memory", "cc", "r8", "r9", "r10", "r11", "xmm0", "xmm1", "xmm2",
        "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10",
        "xmm11", "xmm12", "xmm13", "xmm14", "xmm15", "flags", "fpsr", "st",
        "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");

  ASSERT_EQ(ret, kTestReturnValue);
}

class MockAddressSpaceStatsDumper : public AddressSpaceStatsDumper {
 public:
  MockAddressSpaceStatsDumper() = default;
  void DumpStats(const AddressSpaceStats* address_space_stats) override {}
};

TEST_F(PkeyTest, DumpPkeyPoolStats) {
  if (isolated_globals.pkey == kInvalidPkey) {
    return;
  }

  MockAddressSpaceStatsDumper mock_stats_dumper;
  partition_alloc::internal::AddressPoolManager::GetInstance().DumpStats(
      &mock_stats_dumper);
}

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
