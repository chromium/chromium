// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/stack/stack.h"

#include <memory>
#include <ostream>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

#if PA_BUILDFLAG(IS_LINUX) && \
    (PA_BUILDFLAG(PA_ARCH_CPU_X86) || PA_BUILDFLAG(PA_ARCH_CPU_X86_64))
#include <xmmintrin.h>
#endif

namespace partition_alloc::internal {

namespace {

class PartitionAllocStackTest : public ::testing::Test {
 protected:
  PartitionAllocStackTest() : stack_(std::make_unique<Stack>(GetStackTop())) {}

  Stack* GetStack() const { return stack_.get(); }

 private:
  std::unique_ptr<Stack> stack_;
};

class StackScanner final : public StackVisitor {
 public:
  struct Container {
    std::unique_ptr<int> value;
  };

  StackScanner() : container_(std::make_unique<Container>()) {
    container_->value = std::make_unique<int>();
  }

  void VisitStack(uintptr_t* stack_ptr, uintptr_t* stack_top) final {
    for (; stack_ptr != stack_top; ++stack_ptr) {
      if (*stack_ptr == reinterpret_cast<uintptr_t>(container_->value.get())) {
        found_ = true;
      }
    }
  }

  void Reset() { found_ = false; }
  bool found() const { return found_; }
  int* needle() const { return container_->value.get(); }

 private:
  std::unique_ptr<Container> container_;
  bool found_ = false;
};

}  // namespace

TEST_F(PartitionAllocStackTest, IteratePointersFindsOnStackValue) {
  auto scanner = std::make_unique<StackScanner>();

  // No check that the needle is initially not found as on some platforms it
  // may be part of temporaries after setting it up through StackScanner.
  {
    [[maybe_unused]] int* volatile tmp = scanner->needle();
    GetStack()->IteratePointers(scanner.get());
    EXPECT_TRUE(scanner->found());
  }
}

TEST_F(PartitionAllocStackTest,
       IteratePointersFindsOnStackValuePotentiallyUnaligned) {
  auto scanner = std::make_unique<StackScanner>();

  // No check that the needle is initially not found as on some platforms it
  // may be part of  temporaries after setting it up through StackScanner.
  {
    [[maybe_unused]] char a = 'c';
    [[maybe_unused]] int* volatile tmp = scanner->needle();
    GetStack()->IteratePointers(scanner.get());
    EXPECT_TRUE(scanner->found());
  }
}

namespace {

// Prevent inlining as that would allow the compiler to prove that the parameter
// must not actually be materialized.
//
// Parameter positiosn are explicit to test various calling conventions.
PA_NOINLINE void* RecursivelyPassOnParameterImpl(void* p1,
                                                 void* p2,
                                                 void* p3,
                                                 void* p4,
                                                 void* p5,
                                                 void* p6,
                                                 void* p7,
                                                 void* p8,
                                                 Stack* stack,
                                                 StackVisitor* visitor) {
  if (p1) {
    return RecursivelyPassOnParameterImpl(nullptr, p1, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          stack, visitor);
  } else if (p2) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, p2, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          stack, visitor);
  } else if (p3) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, p3,
                                          nullptr, nullptr, nullptr, nullptr,
                                          stack, visitor);
  } else if (p4) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          p4, nullptr, nullptr, nullptr, stack,
                                          visitor);
  } else if (p5) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          nullptr, p5, nullptr, nullptr, stack,
                                          visitor);
  } else if (p6) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, p6, nullptr, stack,
                                          visitor);
  } else if (p7) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, p7, stack,
                                          visitor);
  } else if (p8) {
    stack->IteratePointers(visitor);
    return p8;
  }
  return nullptr;
}

PA_NOINLINE void* RecursivelyPassOnParameter(size_t num,
                                             void* parameter,
                                             Stack* stack,
                                             StackVisitor* visitor) {
  switch (num) {
    case 0:
      stack->IteratePointers(visitor);
      return parameter;
    case 1:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr,
                                            parameter, stack, visitor);
    case 2:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, parameter,
                                            nullptr, stack, visitor);
    case 3:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            nullptr, parameter, nullptr,
                                            nullptr, stack, visitor);
    case 4:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            parameter, nullptr, nullptr,
                                            nullptr, stack, visitor);
    case 5:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr,
                                            parameter, nullptr, nullptr,
                                            nullptr, nullptr, stack, visitor);
    case 6:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, parameter,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr, stack, visitor);
    case 7:
      return RecursivelyPassOnParameterImpl(nullptr, parameter, nullptr,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr, stack, visitor);
    case 8:
      return RecursivelyPassOnParameterImpl(parameter, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr, stack, visitor);
    default:
      __builtin_unreachable();
  }
  __builtin_unreachable();
}

}  // namespace

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting0) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(0, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting1) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(1, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting2) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(2, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting3) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(3, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting4) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(4, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting5) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(5, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting6) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(6, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting7) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(7, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(PartitionAllocStackTest, IteratePointersFindsParameterNesting8) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(8, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

// The following test uses inline assembly and has been checked to work on clang
// to verify that the stack-scanning trampoline pushes callee-saved registers.
//
// The test uses a macro loop as asm() can only be passed string literals.
#if defined(__clang__) && PA_BUILDFLAG(PA_ARCH_CPU_X86_64) && \
    !PA_BUILDFLAG(IS_WIN)

// Excluded from test: rbp
#define FOR_ALL_CALLEE_SAVED_REGS(V) \
  V(rbx)                             \
  V(r12)                             \
  V(r13)                             \
  V(r14)                             \
  V(r15)

namespace {

extern "C" void IteratePointersNoMangling(Stack* stack, StackVisitor* visitor) {
  stack->IteratePointers(visitor);
}

#define DEFINE_MOVE_INTO(reg)                                         \
  PA_NOINLINE void MoveInto##reg(Stack* local_stack,                  \
                                 StackScanner* local_scanner) {       \
    asm volatile("   mov %0, %%" #reg                                 \
                 "\n mov %1, %%rdi"                                   \
                 "\n mov %2, %%rsi"                                   \
                 "\n call %P3"                                        \
                 "\n mov $0, %%" #reg                                 \
                 :                                                    \
                 : "r"(local_scanner->needle()), "r"(local_stack),    \
                   "r"(local_scanner), "i"(IteratePointersNoMangling) \
                 : "memory", #reg, "rdi", "rsi", "cc");               \
  }

FOR_ALL_CALLEE_SAVED_REGS(DEFINE_MOVE_INTO)

}  // namespace

TEST_F(PartitionAllocStackTest, IteratePointersFindsCalleeSavedRegisters) {
  auto scanner = std::make_unique<StackScanner>();

  // No check that the needle is initially not found as on some platforms it
  // may be part of  temporaries after setting it up through StackScanner.

// First, clear all callee-saved registers.
#define CLEAR_REGISTER(reg) asm("mov $0, %%" #reg : : : #reg);

  FOR_ALL_CALLEE_SAVED_REGS(CLEAR_REGISTER)
#undef CLEAR_REGISTER

  // Keep local raw pointers to keep instruction sequences small below.
  auto* local_stack = GetStack();
  auto* local_scanner = scanner.get();

// Moves |local_scanner->needle()| into a callee-saved register, leaving the
// callee-saved register as the only register referencing the needle.
// (Ignoring implementation-dependent dirty registers/stack.)
#define KEEP_ALIVE_FROM_CALLEE_SAVED(reg)                                 \
  local_scanner->Reset();                                                 \
  MoveInto##reg(local_stack, local_scanner);                              \
  EXPECT_TRUE(local_scanner->found())                                     \
      << "pointer in callee-saved register not found. register: " << #reg \
      << std::endl;

  FOR_ALL_CALLEE_SAVED_REGS(KEEP_ALIVE_FROM_CALLEE_SAVED)
#undef KEEP_ALIVE_FROM_CALLEE_SAVED
#undef FOR_ALL_CALLEE_SAVED_REGS
}

#endif  // defined(__clang__) && PA_BUILDFLAG(PA_ARCH_CPU_X86_64) &&
        // !PA_BUILDFLAG(IS_WIN)

#if PA_BUILDFLAG(IS_LINUX) && \
    (PA_BUILDFLAG(PA_ARCH_CPU_X86) || PA_BUILDFLAG(PA_ARCH_CPU_X86_64))
class CheckStackAlignmentVisitor final : public StackVisitor {
 public:
  void VisitStack(uintptr_t*, uintptr_t*) final {
    // Check that the stack doesn't get misaligned by asm trampolines.
    float f[4] = {0.};
    [[maybe_unused]] volatile auto xmm = ::_mm_load_ps(f);
  }
};

TEST_F(PartitionAllocStackTest, StackAlignment) {
  auto checker = std::make_unique<CheckStackAlignmentVisitor>();
  GetStack()->IteratePointers(checker.get());
}
#endif  // PA_BUILDFLAG(IS_LINUX) && (PA_BUILDFLAG(PA_ARCH_CPU_X86) ||
        // PA_BUILDFLAG(PA_ARCH_CPU_X86_64))

}  // namespace partition_alloc::internal

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
