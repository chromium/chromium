// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <link.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/utsname.h>

#include <algorithm>
#include <array>
#include <cstddef>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android/linker/linker_jni.h"

extern char __executable_start;

extern "C" {

// This function is exported by the dynamic linker but never declared in any
// official header for some architecture/version combinations.
int dl_iterate_phdr(int (*cb)(dl_phdr_info* info, size_t size, void* data),
                    void* data) __attribute__((weak_import));

}  // extern "C"

namespace chromium_android_linker {

namespace {

// Implements the old method of finding library and RELRO ranges by providing a
// callback for use with dl_iterate_phdr(3). Data from the field has shown that
// this method makes library loading significantly slower than
// android_dlopen_ext(), it was replaced by the exuivalent one:
// NativeLibInfo::FindRelroAndLibraryRangesInElf().
class LibraryRangeFinder {
 public:
  explicit LibraryRangeFinder(uintptr_t address) : load_address_(address) {}

  uintptr_t load_address() const { return load_address_; }
  size_t load_size() const { return load_size_; }
  uintptr_t relro_start() const { return relro_start_; }
  size_t relro_size() const { return relro_size_; }

  static int VisitLibraryPhdrs(dl_phdr_info* info,
                               [[maybe_unused]] size_t size,
                               void* data);

 private:
  uintptr_t load_address_;
  size_t load_size_ = 0;
  uintptr_t relro_start_ = 0;
  size_t relro_size_ = 0;
};

// Callback for dl_iterate_phdr(). From program headers (phdr(s)) of a loaded
// library determines its load address, and in case it is equal to
// |load_address()|, extracts the RELRO and size information from
// corresponding phdr(s).
// static
int LibraryRangeFinder::VisitLibraryPhdrs(dl_phdr_info* info,
                                          [[maybe_unused]] size_t size,
                                          void* data) {
  auto* finder = reinterpret_cast<LibraryRangeFinder*>(data);
  ElfW(Addr) lookup_address = static_cast<ElfW(Addr)>(finder->load_address());

  // Use max and min vaddr to compute the library's load size.
  auto min_vaddr = std::numeric_limits<ElfW(Addr)>::max();
  ElfW(Addr) max_vaddr = 0;
  ElfW(Addr) min_relro_vaddr = ~0;
  ElfW(Addr) max_relro_vaddr = 0;

  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  bool is_matching = false;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr)* phdr = &UNSAFE_TODO(info->dlpi_phdr[i]);
    switch (phdr->p_type) {
      case PT_LOAD:
        // See if this segment's load address matches the value passed to
        // android_dlopen_ext as |extinfo.reserved_addr|.
        //
        // Here and below, the virtual address in memory is computed by
        //     address == info->dlpi_addr + program_header->p_vaddr
        // that is, the p_vaddr fields is relative to the object base address.
        // See dl_iterate_phdr(3) for details.
        if (lookup_address == info->dlpi_addr + phdr->p_vaddr) {
          is_matching = true;
        }

        if (phdr->p_vaddr < min_vaddr) {
          min_vaddr = phdr->p_vaddr;
        }
        if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
          max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        }
        break;
      case PT_GNU_RELRO:
        min_relro_vaddr = PageStart(kPageSize, phdr->p_vaddr);
        max_relro_vaddr = phdr->p_vaddr + phdr->p_memsz;

        // As of 2020-11 in libmonochrome.so RELRO is covered by a LOAD segment.
        // It is not clear whether this property is going to be guaranteed in
        // the future. Include the RELRO segment as part of the 'load size'.
        // This way a potential future change in layout of LOAD segments would
        // not open address space for racy mmap(MAP_FIXED).
        if (min_relro_vaddr < min_vaddr) {
          min_vaddr = min_relro_vaddr;
        }
        if (max_vaddr < max_relro_vaddr) {
          max_vaddr = max_relro_vaddr;
        }
        break;
      default:
        break;
    }
  }

  // Fill out size and relro information if there was a match.
  if (is_matching) {
    finder->load_size_ =
        PageEnd(kPageSize, max_vaddr) - PageStart(kPageSize, min_vaddr);
    finder->relro_size_ = PageEnd(kPageSize, max_relro_vaddr) -
                          PageStart(kPageSize, min_relro_vaddr);
    finder->relro_start_ =
        info->dlpi_addr + PageStart(kPageSize, min_relro_vaddr);

    return 1;
  }

  return 0;
}

}  // namespace

// These tests get linked with base_unittests and leave JNI uninitialized. The
// tests must not execute any parts relying on initialization with JNI_Onload().

class LinkerTest : public testing::Test {
 public:
  LinkerTest() = default;
  ~LinkerTest() override = default;
};

// Checks that NativeLibInfo::CreateSharedRelroFd() creates a shared memory
// region that is 'sealed' as read-only.
TEST_F(LinkerTest, CreatedRegionIsSealed) {
  // Fill a synthetic RELRO region with 0xEE in private anonynous memory.
  constexpr size_t kRelroSize = 1 << 21;  // 2 MiB.
  void* relro_address = mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, relro_address);
  NativeLibInfo lib_info = {0, 0};
  lib_info.set_relro_info_for_testing(
      reinterpret_cast<uintptr_t>(relro_address), kRelroSize);
  UNSAFE_TODO(memset(relro_address, 0xEE, kRelroSize));

  // Create shared RELRO.
  ASSERT_EQ(true, lib_info.CreateSharedRelroFdForTesting());
  int relro_fd = lib_info.get_relro_fd_for_testing();
  ASSERT_NE(-1, relro_fd);
  base::ScopedFD scoped_fd(relro_fd);

  // Check that a read-only mapping contains the data originally filled in.
  void* ro_address =
      mmap(nullptr, kRelroSize, PROT_READ, MAP_SHARED, relro_fd, 0);
  ASSERT_NE(MAP_FAILED, ro_address);
  EXPECT_EQ(0xEEEEEEEEU, *reinterpret_cast<uint32_t*>(ro_address));
  int not_equal = UNSAFE_TODO(memcmp(relro_address, ro_address, kRelroSize));
  EXPECT_EQ(0, not_equal);
  munmap(ro_address, kRelroSize);

  // Check that attempts to mmap with PROT_WRITE fail.
  EXPECT_EQ(MAP_FAILED, mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_SHARED, relro_fd, 0));
  EXPECT_EQ(MAP_FAILED, mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE, relro_fd, 0));
  EXPECT_EQ(MAP_FAILED,
            mmap(nullptr, kRelroSize, PROT_WRITE, MAP_SHARED, relro_fd, 0));
  EXPECT_EQ(MAP_FAILED,
            mmap(nullptr, kRelroSize, PROT_WRITE, MAP_PRIVATE, relro_fd, 0));
}

TEST_F(LinkerTest, FindReservedMemoryRegion) {
  size_t address, size;

  // Find the existing reservation in the current process. The unittest runner
  // is forked from the system zygote. The reservation should be found when
  // running on recent Android releases, where it is made by the
  // reserveAddressSpaceInZygote().
  bool found_reservation = FindWebViewReservation(&address, &size);

  if (found_reservation) {
    // Check that the size is at least the minimum reserved by Android, as of
    // 2021-04.
    EXPECT_LE(130U * 1024 * 1024, size);
    return;
  }

  // TODO(crbug.com/40774803): Check that only non-low-end Android Q+ devices
  // reach this point.

  // Create a properly named synthetic region with a size smaller than a real
  // library would need, but still aligned well.
  static const size_t kSize = 19U * 1024 * 1024;
  void* synthetic_region_start =
      mmap(nullptr, kSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, synthetic_region_start);
  prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, synthetic_region_start, kSize,
        "[anon:libwebview reservation]");

  // Now the region must be found.
  EXPECT_TRUE(FindWebViewReservation(&address, &size));
  EXPECT_EQ(kSize, size);
  EXPECT_EQ(reinterpret_cast<void*>(address), synthetic_region_start);
  munmap(synthetic_region_start, kSize);
}

TEST_F(LinkerTest, FindLibraryRanges) {
  static int var_inside = 3;

  NativeLibInfo lib_info = {0, 0};
  uintptr_t executable_start = reinterpret_cast<uintptr_t>(&__executable_start);
  lib_info.set_load_address(executable_start);

  EXPECT_TRUE(lib_info.FindRelroAndLibraryRangesInElfForTesting());
  EXPECT_EQ(executable_start, lib_info.load_address());

  uintptr_t inside_library = reinterpret_cast<uintptr_t>(&var_inside);
  EXPECT_LE(executable_start, inside_library);
  EXPECT_LE(inside_library,
            lib_info.load_address() + lib_info.get_load_size_for_testing());

  EXPECT_LE(lib_info.load_address(), lib_info.get_relro_start_for_testing());
  EXPECT_LE(lib_info.get_relro_start_for_testing(),
            lib_info.load_address() + lib_info.get_load_size_for_testing());
}

TEST_F(LinkerTest, FindLibraryRangesWhenLoadAddressWasReset) {
  NativeLibInfo other_lib_info = {0, 0};
  uintptr_t executable_start = reinterpret_cast<uintptr_t>(&__executable_start);
  other_lib_info.set_load_address(executable_start);
  other_lib_info.set_relro_fd_for_testing(123);
  NativeLibInfo lib_info = {0, 0};
  EXPECT_FALSE(lib_info.CompareRelroAndReplaceItBy(other_lib_info));
}

// Check that discovering RELRO segment address ranges and the DSO ranges agrees
// with the method based on dl_iterate_phdr(3). The check is performed on the
// test library, not on libmonochrome.
TEST_F(LinkerTest, LibraryRangesViaIteratePhdr) {
  // Find the ranges using dl_iterate_phdr().
  if (!dl_iterate_phdr) {
    ASSERT_TRUE(false) << "dl_iterate_phdr() not found";
  }
  uintptr_t executable_start = reinterpret_cast<uintptr_t>(&__executable_start);
  LibraryRangeFinder finder(executable_start);
  ASSERT_EQ(1, dl_iterate_phdr(&LibraryRangeFinder::VisitLibraryPhdrs,
                               reinterpret_cast<void*>(&finder)));
  ASSERT_LE(finder.relro_start() + finder.relro_size(),
            finder.load_address() + finder.load_size());

  // Find the ranges by parsing ELF.
  NativeLibInfo lib_info2 = {0, 0};
  lib_info2.set_load_address(executable_start);
  EXPECT_TRUE(lib_info2.FindRelroAndLibraryRangesInElfForTesting());

  // Compare results.
  EXPECT_EQ(finder.load_address(), lib_info2.load_address());
  EXPECT_EQ(finder.load_size(), lib_info2.get_load_size_for_testing());
  EXPECT_EQ(finder.relro_start(), lib_info2.get_relro_start_for_testing());
}

TEST_F(LinkerTest, CompareRelroAndReplaceCommonPrefix) {
  constexpr size_t kMaxPageSize = 65536;
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  ASSERT_LE(kPageSize, kMaxPageSize);
  const size_t kNumRelroPages = 3;
  const size_t kRelroSize = kNumRelroPages * kPageSize;
  const size_t kTotalSize = kPageSize + kRelroSize;

  // Map more memory than required to avoid UNSAFE_BUFFERS for mapping_span.
  using MappingBuffer =
      std::array<uint8_t, (1 + kNumRelroPages) * kMaxPageSize>;
  void* mapping = mmap(nullptr, sizeof(MappingBuffer), PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, mapping);
  uintptr_t base_address = reinterpret_cast<uintptr_t>(mapping);
  base::span<uint8_t> mapping_span =
      base::span(*static_cast<MappingBuffer*>(mapping)).first(kTotalSize);

  // Set up a minimal ELF header and program headers in the first page so
  // FindRelroAndLibraryRangesInElf() succeeds on the synthetic mapping.
  struct SyntheticElfHeader {
    ElfW(Ehdr) ehdr;
    std::array<ElfW(Phdr), 2> phdrs;
  };
  auto* elf = reinterpret_cast<SyntheticElfHeader*>(mapping_span.data());
  base::span(elf->ehdr.e_ident)
      .copy_prefix_from(base::byte_span_from_cstring(ELFMAG));
  elf->ehdr.e_ident[EI_CLASS] = (sizeof(void*) == 8) ? ELFCLASS64 : ELFCLASS32;
  elf->ehdr.e_phoff = offsetof(SyntheticElfHeader, phdrs);
  elf->ehdr.e_phnum = elf->phdrs.size();
  elf->phdrs[0] = {.p_type = PT_LOAD, .p_vaddr = 0, .p_memsz = kTotalSize};
  elf->phdrs[1] = {
      .p_type = PT_GNU_RELRO, .p_vaddr = kPageSize, .p_memsz = kRelroSize};

  base::span<uint8_t> relro_span = mapping_span.subspan(kPageSize, kRelroSize);
  base::span<uint8_t> third_page = relro_span.subspan(2 * kPageSize, kPageSize);
  base::span<uint8_t> third_page_second_half =
      third_page.subspan(kPageSize / 2);

  // Fill all 3 RELRO pages with 0xEE, except the second half of the 3rd page
  // which has 0xFF in the incoming shared RELRO.
  std::ranges::fill(relro_span, 0xEE);
  std::ranges::fill(third_page_second_half, 0xFF);

  NativeLibInfo other_lib_info = {nullptr, nullptr};
  other_lib_info.set_load_address(base_address);
  ASSERT_TRUE(other_lib_info.FindRelroAndLibraryRangesInElfForTesting());
  ASSERT_TRUE(other_lib_info.CreateSharedRelroFdForTesting());
  int relro_fd = other_lib_info.get_relro_fd_for_testing();
  ASSERT_NE(-1, relro_fd);
  base::ScopedFD scoped_fd(relro_fd);

  // Simulate process-specific relocations (e.g. R_AARCH64_TLSDESC) in the
  // second half of the 3rd RELRO page of the local library.
  std::ranges::fill(third_page_second_half, 0xAA);

  NativeLibInfo lib_info = {nullptr, nullptr};
  lib_info.set_load_address(base_address);
  EXPECT_TRUE(lib_info.CompareRelroAndReplaceItBy(other_lib_info));

  // The first two pages (common prefix truncated to page boundary) should now
  // be backed by the sealed read-only shared memory FD, so mprotect(PROT_WRITE)
  // must fail on them.
  EXPECT_EQ(-1,
            mprotect(relro_span.data(), 2 * kPageSize, PROT_READ | PROT_WRITE));
  EXPECT_EQ(0xEE, relro_span[0]);

  // The 3rd page should not have been replaced: it retains the local 0xAA data
  // in its second half and remains private anonymous memory.
  EXPECT_EQ(0xEE, third_page[0]);
  EXPECT_EQ(0xAA, third_page_second_half[0]);
  EXPECT_EQ(0, mprotect(third_page.data(), third_page.size(),
                        PROT_READ | PROT_WRITE));

  // If the very first page of RELRO differs, sharing should fail.
  base::span<uint8_t> first_page = relro_span.first(kPageSize);
  EXPECT_EQ(0, mprotect(mapping_span.data(), mapping_span.size(), PROT_READ));
  void* private_first_page =
      mmap(first_page.data(), first_page.size(), PROT_READ | PROT_WRITE,
           MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, private_first_page);
  std::ranges::fill(first_page, 0x11);
  EXPECT_FALSE(lib_info.CompareRelroAndReplaceItBy(other_lib_info));
  EXPECT_EQ(0x11, first_page[0]);

  munmap(mapping, sizeof(MappingBuffer));
}

}  // namespace chromium_android_linker
