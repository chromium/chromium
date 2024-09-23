// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and use spans.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/chrome_unwinder_android.h"

#include "base/memory/aligned_memory.h"
#include "base/profiler/chrome_unwind_info_android.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/ranges/algorithm.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerIncrementMinValue) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b00000000;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x10000004ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerIncrementMidValue) {
  // xxxxxx = 4; vsp = vsp + (4 << 2) + 4 = vsp + 16 + 4 = vsp + 0x14.
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b00000100;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x10000014ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerIncrementMaxValue) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b00111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x10000100ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerIncrementOverflow) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b00111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0xffffffff;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kAborted);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0xffffffff, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerDecrementMinValue) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b01000000;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x0ffffffcul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerDecrementMidValue) {
  // xxxxxx = 4; vsp = vsp - (4 << 2) - 4 = vsp - 16 - 4 = vsp - 0x14.
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b01000100;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x0fffffecul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerDecrementMaxValue) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b01111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x0fffff00ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerDecrementUnderflow) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b01111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x00000000;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kAborted);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x0ul, thread_context.arm_sp);
}

using ChromeAndroidUnwindSetStackPointerFromRegisterValueTest =
    ::testing::TestWithParam<uint8_t>;

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeAndroidUnwindSetStackPointerFromRegisterValueTest,
    // The function should set all registers except
    // - callee saved registers (r0, r1, r2, r3)
    // - sp (r13)
    // - pc (r15)
    ::testing::Values(4, 5, 6, 7, 8, 9, 10, 11, 12, 14));

TEST_P(ChromeAndroidUnwindSetStackPointerFromRegisterValueTest,
       TestSetStackPointerFromRegisterValue) {
  const uint8_t register_index = GetParam();

  RegisterContext thread_context = {};
  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;  // r11
  thread_context.arm_ip = 112;  // r12
  thread_context.arm_lr = 114;  // r14

  const uint8_t instruction = 0b10010000 + register_index;
  const uint8_t* current_instruction = &instruction;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(100ul + register_index, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest, TestCompleteWithNoPriorPCUpdate) {
  RegisterContext thread_context = {};
  thread_context.arm_lr = 114;  // r14
  thread_context.arm_pc = 115;  // r15
  const uint8_t instruction = 0b10110000;
  const uint8_t* current_instruction = &instruction;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kCompleted);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(114ul, thread_context.arm_pc);
}

TEST(ChromeAndroidUnwindInstructionTest, TestCompleteWithPriorPCUpdate) {
  RegisterContext thread_context = {};
  thread_context.arm_lr = 114;  // r14
  thread_context.arm_pc = 115;  // r15
  const uint8_t instruction = 0b10110000;
  const uint8_t* current_instruction = &instruction;
  bool pc_was_updated = true;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kCompleted);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(115ul, thread_context.arm_pc);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestPopDiscontinuousRegistersIncludingPC) {
  RegisterContext thread_context = {};

  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;
  thread_context.arm_ip = 112;
  thread_context.arm_lr = 113;
  thread_context.arm_pc = 114;

  // Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
  const uintptr_t stack[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  thread_context.arm_sp = reinterpret_cast<uintptr_t>(&stack[0]);
  // Pop r15, r12, r8, r4.
  const uint8_t instruction[] = {0b10001001, 0b00010001};
  const uint8_t* current_instruction = instruction;

  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_TRUE(pc_was_updated);
  ASSERT_EQ(current_instruction, instruction + 2);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack[0] + 4), thread_context.arm_sp);

  EXPECT_EQ(100ul, thread_context.arm_r0);
  EXPECT_EQ(101ul, thread_context.arm_r1);
  EXPECT_EQ(102ul, thread_context.arm_r2);
  EXPECT_EQ(103ul, thread_context.arm_r3);
  EXPECT_EQ(1ul, thread_context.arm_r4);
  EXPECT_EQ(105ul, thread_context.arm_r5);
  EXPECT_EQ(106ul, thread_context.arm_r6);
  EXPECT_EQ(107ul, thread_context.arm_r7);
  EXPECT_EQ(2ul, thread_context.arm_r8);
  EXPECT_EQ(109ul, thread_context.arm_r9);
  EXPECT_EQ(110ul, thread_context.arm_r10);
  EXPECT_EQ(111ul, thread_context.arm_fp);
  EXPECT_EQ(3ul, thread_context.arm_ip);
  EXPECT_EQ(113ul, thread_context.arm_lr);
  EXPECT_EQ(4ul, thread_context.arm_pc);
}

TEST(ChromeAndroidUnwindInstructionTest, TestPopDiscontinuousRegisters) {
  RegisterContext thread_context = {};

  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;
  thread_context.arm_ip = 112;
  thread_context.arm_lr = 113;
  thread_context.arm_pc = 114;

  // Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
  const uintptr_t stack[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  thread_context.arm_sp = reinterpret_cast<uintptr_t>(&stack[0]);
  // Pop r12, r8, r4.
  const uint8_t instruction[] = {0b10000001, 0b00010001};
  const uint8_t* current_instruction = instruction;

  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, instruction + 2);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack[0] + 3), thread_context.arm_sp);

  EXPECT_EQ(100ul, thread_context.arm_r0);
  EXPECT_EQ(101ul, thread_context.arm_r1);
  EXPECT_EQ(102ul, thread_context.arm_r2);
  EXPECT_EQ(103ul, thread_context.arm_r3);
  EXPECT_EQ(1ul, thread_context.arm_r4);
  EXPECT_EQ(105ul, thread_context.arm_r5);
  EXPECT_EQ(106ul, thread_context.arm_r6);
  EXPECT_EQ(107ul, thread_context.arm_r7);
  EXPECT_EQ(2ul, thread_context.arm_r8);
  EXPECT_EQ(109ul, thread_context.arm_r9);
  EXPECT_EQ(110ul, thread_context.arm_r10);
  EXPECT_EQ(111ul, thread_context.arm_fp);
  EXPECT_EQ(3ul, thread_context.arm_ip);
  EXPECT_EQ(113ul, thread_context.arm_lr);
  EXPECT_EQ(114ul, thread_context.arm_pc);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestPopDiscontinuousRegistersOverflow) {
  RegisterContext thread_context = {};

  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;
  thread_context.arm_ip = 112;
  thread_context.arm_lr = 113;
  thread_context.arm_pc = 114;

  // Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
  thread_context.arm_sp = 0xffffffff;
  // Pop r15, r12, r8, r4.
  const uint8_t instruction[] = {0b10001001, 0b00010001};
  const uint8_t* current_instruction = instruction;

  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kAborted);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, instruction + 2);
  EXPECT_EQ(0xffffffff, thread_context.arm_sp);

  EXPECT_EQ(100ul, thread_context.arm_r0);
  EXPECT_EQ(101ul, thread_context.arm_r1);
  EXPECT_EQ(102ul, thread_context.arm_r2);
  EXPECT_EQ(103ul, thread_context.arm_r3);
  EXPECT_EQ(104ul, thread_context.arm_r4);
  EXPECT_EQ(105ul, thread_context.arm_r5);
  EXPECT_EQ(106ul, thread_context.arm_r6);
  EXPECT_EQ(107ul, thread_context.arm_r7);
  EXPECT_EQ(108ul, thread_context.arm_r8);
  EXPECT_EQ(109ul, thread_context.arm_r9);
  EXPECT_EQ(110ul, thread_context.arm_r10);
  EXPECT_EQ(111ul, thread_context.arm_fp);
  EXPECT_EQ(112ul, thread_context.arm_ip);
  EXPECT_EQ(113ul, thread_context.arm_lr);
  EXPECT_EQ(114ul, thread_context.arm_pc);
}

TEST(ChromeAndroidUnwindInstructionTest, TestRefuseToUnwind) {
  RegisterContext thread_context = {};

  const uint8_t instruction[] = {0b10000000, 0b0};
  const uint8_t* current_instruction = instruction;

  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kAborted);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, instruction + 2);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestPopRegistersIncludingR14MinRegisters) {
  RegisterContext thread_context = {};

  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;
  thread_context.arm_ip = 112;
  thread_context.arm_lr = 113;

  // Popping r4 - r[4 + nnn], r14, at most 9 registers.
  // r14 = lr
  const uintptr_t stack[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  thread_context.arm_sp = reinterpret_cast<uintptr_t>(&stack[0]);
  const uint8_t instruction = 0b10101000;
  const uint8_t* current_instruction = &instruction;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack[0] + 2), thread_context.arm_sp);

  EXPECT_EQ(100ul, thread_context.arm_r0);
  EXPECT_EQ(101ul, thread_context.arm_r1);
  EXPECT_EQ(102ul, thread_context.arm_r2);
  EXPECT_EQ(103ul, thread_context.arm_r3);
  EXPECT_EQ(1ul, thread_context.arm_r4);
  EXPECT_EQ(105ul, thread_context.arm_r5);
  EXPECT_EQ(106ul, thread_context.arm_r6);
  EXPECT_EQ(107ul, thread_context.arm_r7);
  EXPECT_EQ(108ul, thread_context.arm_r8);
  EXPECT_EQ(109ul, thread_context.arm_r9);
  EXPECT_EQ(110ul, thread_context.arm_r10);
  EXPECT_EQ(111ul, thread_context.arm_fp);
  EXPECT_EQ(112ul, thread_context.arm_ip);
  EXPECT_EQ(2ul, thread_context.arm_lr);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestPopRegistersIncludingR14MidRegisters) {
  RegisterContext thread_context = {};

  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;
  thread_context.arm_ip = 112;
  thread_context.arm_lr = 113;

  // Popping r4 - r[4 + nnn], r14, at most 9 registers.
  // r14 = lr
  const uintptr_t stack[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  thread_context.arm_sp = reinterpret_cast<uintptr_t>(&stack[0]);
  const uint8_t instruction = 0b10101100;  // Pop r4-r8, r14.
  const uint8_t* current_instruction = &instruction;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack[0] + 6), thread_context.arm_sp);

  EXPECT_EQ(100ul, thread_context.arm_r0);
  EXPECT_EQ(101ul, thread_context.arm_r1);
  EXPECT_EQ(102ul, thread_context.arm_r2);
  EXPECT_EQ(103ul, thread_context.arm_r3);
  EXPECT_EQ(1ul, thread_context.arm_r4);
  EXPECT_EQ(2ul, thread_context.arm_r5);
  EXPECT_EQ(3ul, thread_context.arm_r6);
  EXPECT_EQ(4ul, thread_context.arm_r7);
  EXPECT_EQ(5ul, thread_context.arm_r8);
  EXPECT_EQ(109ul, thread_context.arm_r9);
  EXPECT_EQ(110ul, thread_context.arm_r10);
  EXPECT_EQ(111ul, thread_context.arm_fp);
  EXPECT_EQ(112ul, thread_context.arm_ip);
  EXPECT_EQ(6ul, thread_context.arm_lr);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestPopRegistersIncludingR14MaxRegisters) {
  RegisterContext thread_context = {};

  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;
  thread_context.arm_ip = 112;
  thread_context.arm_lr = 113;

  // Popping r4 - r[4 + nnn], r14, at most 9 registers.
  // r14 = lr
  const uintptr_t stack[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  thread_context.arm_sp = reinterpret_cast<uintptr_t>(&stack[0]);
  const uint8_t instruction = 0b10101111;  // Pop r4 - r11, r14.
  const uint8_t* current_instruction = &instruction;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&stack[0] + 9), thread_context.arm_sp);

  EXPECT_EQ(100ul, thread_context.arm_r0);
  EXPECT_EQ(101ul, thread_context.arm_r1);
  EXPECT_EQ(102ul, thread_context.arm_r2);
  EXPECT_EQ(103ul, thread_context.arm_r3);
  EXPECT_EQ(1ul, thread_context.arm_r4);
  EXPECT_EQ(2ul, thread_context.arm_r5);
  EXPECT_EQ(3ul, thread_context.arm_r6);
  EXPECT_EQ(4ul, thread_context.arm_r7);
  EXPECT_EQ(5ul, thread_context.arm_r8);
  EXPECT_EQ(6ul, thread_context.arm_r9);
  EXPECT_EQ(7ul, thread_context.arm_r10);
  EXPECT_EQ(8ul, thread_context.arm_fp);
  EXPECT_EQ(112ul, thread_context.arm_ip);
  EXPECT_EQ(9ul, thread_context.arm_lr);
}

TEST(ChromeAndroidUnwindInstructionTest, TestPopRegistersIncludingR14Overflow) {
  RegisterContext thread_context = {};

  thread_context.arm_r0 = 100;
  thread_context.arm_r1 = 101;
  thread_context.arm_r2 = 102;
  thread_context.arm_r3 = 103;
  thread_context.arm_r4 = 104;
  thread_context.arm_r5 = 105;
  thread_context.arm_r6 = 106;
  thread_context.arm_r7 = 107;
  thread_context.arm_r8 = 108;
  thread_context.arm_r9 = 109;
  thread_context.arm_r10 = 110;
  thread_context.arm_fp = 111;
  thread_context.arm_ip = 112;
  thread_context.arm_lr = 113;

  // Popping r4 - r[4 + nnn], r14, at most 9 registers.
  // r14 = lr
  thread_context.arm_sp = 0xffffffff;
  const uint8_t instruction = 0b10101111;  // Pop r4 - r11, r14.
  const uint8_t* current_instruction = &instruction;
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kAborted);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0xffffffff, thread_context.arm_sp);

  EXPECT_EQ(100ul, thread_context.arm_r0);
  EXPECT_EQ(101ul, thread_context.arm_r1);
  EXPECT_EQ(102ul, thread_context.arm_r2);
  EXPECT_EQ(103ul, thread_context.arm_r3);
  EXPECT_EQ(104ul, thread_context.arm_r4);
  EXPECT_EQ(105ul, thread_context.arm_r5);
  EXPECT_EQ(106ul, thread_context.arm_r6);
  EXPECT_EQ(107ul, thread_context.arm_r7);
  EXPECT_EQ(108ul, thread_context.arm_r8);
  EXPECT_EQ(109ul, thread_context.arm_r9);
  EXPECT_EQ(110ul, thread_context.arm_r10);
  EXPECT_EQ(111ul, thread_context.arm_fp);
  EXPECT_EQ(112ul, thread_context.arm_ip);
  EXPECT_EQ(113ul, thread_context.arm_lr);
}

TEST(ChromeAndroidUnwindInstructionTest, TestBigStackPointerIncrementMinValue) {
  RegisterContext thread_context = {};
  thread_context.arm_sp = 0x10000000;

  const uint8_t increment_0[] = {
      0b10110010,
      0b00000000,
  };
  const uint8_t* current_instruction = &increment_0[0];
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, increment_0 + sizeof(increment_0));
  // vsp + 0x204 + (0 << 2)
  // = vsp + 0x204
  EXPECT_EQ(0x10000204ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest, TestBigStackPointerIncrementMidValue) {
  RegisterContext thread_context = {};
  thread_context.arm_sp = 0x10000000;

  const uint8_t increment_4[] = {
      0b10110010,
      0b00000100,
  };
  const uint8_t* current_instruction = &increment_4[0];

  // vsp + 0x204 + (4 << 2)
  // = vsp + 0x204 + 0x10
  // = vsp + 0x214
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, increment_4 + sizeof(increment_4));
  EXPECT_EQ(0x10000214ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestBigStackPointerIncrementLargeValue) {
  RegisterContext thread_context = {};
  thread_context.arm_sp = 0x10000000;

  const uint8_t increment_128[] = {
      0b10110010,
      0b10000000,
      0b00000001,
  };
  const uint8_t* current_instruction = &increment_128[0];
  // vsp + 0x204 + (128 << 2)
  // = vsp + 0x204 + 512
  // = vsp + 0x204 + 0x200
  // = vsp + 0x404
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kInstructionPending);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction, increment_128 + sizeof(increment_128));
  EXPECT_EQ(0x10000404ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest, TestBigStackPointerIncrementOverflow) {
  RegisterContext thread_context = {};
  thread_context.arm_sp = 0xffffffff;

  const uint8_t increment_overflow[] = {
      0b10110010,
      0b10000000,
      0b00000001,
  };  // ULEB128 = 128
  const uint8_t* current_instruction = &increment_overflow[0];
  bool pc_was_updated = false;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, pc_was_updated,
                                     &thread_context),
            UnwindInstructionResult::kAborted);
  EXPECT_FALSE(pc_was_updated);
  ASSERT_EQ(current_instruction,
            increment_overflow + sizeof(increment_overflow));
  EXPECT_EQ(0xfffffffful, thread_context.arm_sp);
}

TEST(ChromeUnwinderAndroidTest,
     TestFunctionOffsetTableLookupExactMatchingOffset) {
  const uint8_t function_offset_table[] = {
      // Function 1: [(130, 2), (128, 3), (0, 4)]
      // offset = 130
      0b10000010,
      0b00000001,
      // unwind index = 2
      0b00000010,
      // offset = 128
      0b10000000,
      0b00000001,
      // unwind index = 3
      0b00000011,
      // offset = 0
      0b00000000,
      // unwind index = 4
      0b00000100,
  };

  EXPECT_EQ(3ul, GetFirstUnwindInstructionIndexFromFunctionOffsetTableEntry(
                     &function_offset_table[0],
                     /* instruction_offset_from_function_start */ 128));
}

TEST(ChromeUnwinderAndroidTest,
     TestFunctionOffsetTableLookupNonExactMatchingOffset) {
  const uint8_t function_offset_table[] = {
      // Function 1: [(130, 2), (128, 3), (0, 4)]
      // offset = 130
      0b10000010,
      0b00000001,
      // unwind index = 2
      0b00000010,
      // offset = 128
      0b10000000,
      0b00000001,
      // unwind index = 3
      0b00000011,
      // offset = 0
      0b00000000,
      // unwind index = 4
      0b00000100,
  };

  EXPECT_EQ(3ul, GetFirstUnwindInstructionIndexFromFunctionOffsetTableEntry(
                     &function_offset_table[0],
                     /* instruction_offset_from_function_start */ 129));
}

TEST(ChromeUnwinderAndroidTest, TestFunctionOffsetTableLookupZeroOffset) {
  const uint8_t function_offset_table[] = {
      // Function 1: [(130, 2), (128, 3), (0, 4)]
      // offset = 130
      0b10000010,
      0b00000001,
      // unwind index = 2
      0b00000010,
      // offset = 128
      0b10000000,
      0b00000001,
      // unwind index = 3
      0b00000011,
      // offset = 0
      0b00000000,
      // unwind index = 4
      0b00000100,
  };

  EXPECT_EQ(4ul, GetFirstUnwindInstructionIndexFromFunctionOffsetTableEntry(
                     &function_offset_table[0],
                     /* instruction_offset_from_function_start */ 0));
}

TEST(ChromeUnwinderAndroidTest, TestAddressTableLookupEntryInPage) {
  const uint32_t page_start_instructions[] = {0, 2};
  const FunctionTableEntry function_offset_table_indices[] = {
      // Page 0
      {
          /* function_start_address_page_instruction_offset */ 0,
          /* function_offset_table_byte_index */ 20,
      },
      {
          /* function_start_address_page_instruction_offset */ 4,
          /* function_offset_table_byte_index */ 40,
      },
      // Page 1
      {
          /* function_start_address_page_instruction_offset */ 6,
          /* function_offset_table_byte_index */ 70,
      },
  };

  {
    const uint32_t page_number = 0;
    const uint32_t page_instruction_offset = 4;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_NE(std::nullopt, entry_found);
    EXPECT_EQ(0, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(40ul, entry_found->function_offset_table_byte_index);
  }

  {
    const uint32_t page_number = 0;
    const uint32_t page_instruction_offset = 50;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_NE(std::nullopt, entry_found);
    EXPECT_EQ(46, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(40ul, entry_found->function_offset_table_byte_index);
  }

  // Lookup last instruction in last function.
  {
    const uint32_t page_number = 1;
    const uint32_t page_instruction_offset = 0xffff;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_NE(std::nullopt, entry_found);
    // 0xffff - 6 = 0xfff9.
    EXPECT_EQ(0xfff9, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(70ul, entry_found->function_offset_table_byte_index);
  }
}

TEST(ChromeUnwinderAndroidTest, TestAddressTableLookupEmptyPage) {
  const uint32_t page_start_instructions[] = {0, 1, 1};
  const FunctionTableEntry function_offset_table_indices[] = {
      // Page 0
      {
          /* function_start_address_page_instruction_offset */ 0,
          /* function_offset_table_byte_index */ 20,
      },
      // Page 1 is empty
      // Page 2
      {
          /* function_start_address_page_instruction_offset */ 6,
          /* function_offset_table_byte_index */ 70,
      },
  };

  const uint32_t page_number = 1;
  const uint32_t page_instruction_offset = 4;
  const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
      page_start_instructions, function_offset_table_indices,
      /* instruction_offset */ (page_instruction_offset << 1) +
          (page_number << 17));
  ASSERT_NE(std::nullopt, entry_found);
  EXPECT_EQ(0x10004, entry_found->instruction_offset_from_function_start);
  EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
}

TEST(ChromeUnwinderAndroidTest, TestAddressTableLookupInvalidIntructionOffset) {
  const uint32_t page_start_instructions[] = {0, 1};
  const FunctionTableEntry function_offset_table_indices[] = {
      // Page 0
      // This function spans from page 0 offset 0 to page 1 offset 5.
      {
          /* function_start_address_page_instruction_offset */ 0,
          /* function_offset_table_byte_index */ 20,
      },
      // Page 1
      {
          /* function_start_address_page_instruction_offset */ 6,
          /* function_offset_table_byte_index */ 70,
      },
  };

  // Instruction offset lies after last page on page table.
  {
    const uint32_t page_number = 50;
    const uint32_t page_instruction_offset = 6;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_EQ(std::nullopt, entry_found);
  }
  {
    const uint32_t page_number = 2;
    const uint32_t page_instruction_offset = 0;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_EQ(std::nullopt, entry_found);
  }
}

TEST(ChromeUnwinderAndroidTest,
     TestAddressTableLookupOnSecondPageOfFunctionSpanningPageBoundary) {
  const uint32_t page_start_instructions[] = {0, 1, 2};
  const FunctionTableEntry function_offset_table_indices[] = {
      // Page 0
      {
          /* function_start_address_page_instruction_offset */ 0,
          /* function_offset_table_byte_index */ 20,
      },
      // Page 1
      {
          /* function_start_address_page_instruction_offset */ 6,
          /* function_offset_table_byte_index */ 70,
      },
      // Page 2
      {
          /* function_start_address_page_instruction_offset */ 10,
          /* function_offset_table_byte_index */ 80,
      }};

  const uint32_t page_number = 1;
  const uint32_t page_instruction_offset = 4;
  const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
      page_start_instructions, function_offset_table_indices,
      /* instruction_offset */ (page_instruction_offset << 1) +
          (page_number << 17));
  ASSERT_NE(std::nullopt, entry_found);
  EXPECT_EQ(0x10004, entry_found->instruction_offset_from_function_start);
  EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
}

TEST(ChromeUnwinderAndroidTest,
     TestAddressTableLookupWithinFunctionSpanningMultiplePages) {
  const uint32_t page_start_instructions[] = {0, 1, 1, 1};
  const FunctionTableEntry function_offset_table_indices[] = {
      // Page 0
      // This function spans from page 0 offset 0 to page 3 offset 5.
      {
          /* function_start_address_page_instruction_offset */ 0,
          /* function_offset_table_byte_index */ 20,
      },
      // Page 1 is empty
      // Page 2 is empty
      // Page 3
      {
          /* function_start_address_page_instruction_offset */ 6,
          /* function_offset_table_byte_index */ 70,
      },
  };

  {
    const uint32_t page_number = 0;
    const uint32_t page_instruction_offset = 4;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_NE(std::nullopt, entry_found);
    EXPECT_EQ(0x4, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
  }
  {
    const uint32_t page_number = 1;
    const uint32_t page_instruction_offset = 4;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_NE(std::nullopt, entry_found);
    EXPECT_EQ(0x10004, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
  }
  {
    const uint32_t page_number = 2;
    const uint32_t page_instruction_offset = 4;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_NE(std::nullopt, entry_found);
    EXPECT_EQ(0x20004, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
  }
  {
    const uint32_t page_number = 3;
    const uint32_t page_instruction_offset = 4;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_NE(std::nullopt, entry_found);
    EXPECT_EQ(0x30004, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
  }
}

// Utility function to add a single native module during test setup. Returns
// a pointer to the provided module.
const ModuleCache::Module* AddNativeModule(
    ModuleCache* cache,
    std::unique_ptr<const ModuleCache::Module> module) {
  const ModuleCache::Module* module_ptr = module.get();
  cache->AddCustomNativeModule(std::move(module));
  return module_ptr;
}

TEST(ChromeUnwinderAndroidTest, CanUnwindFrom) {
  const uint32_t page_table[] = {0};
  const FunctionTableEntry function_table[] = {{0, 0}};
  const uint8_t function_offset_table[] = {0};
  const uint8_t unwind_instruction_table[] = {0};
  auto dummy_unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  auto chrome_module = std::make_unique<TestModule>(0x1000, 0x500);
  auto non_chrome_module = std::make_unique<TestModule>(0x2000, 0x500);

  ModuleCache module_cache;
  ChromeUnwinderAndroid unwinder(dummy_unwind_info,
                                 chrome_module->GetBaseAddress(),
                                 /* text_section_start_address */
                                 chrome_module->GetBaseAddress() + 4);
  unwinder.Initialize(&module_cache);

  EXPECT_TRUE(unwinder.CanUnwindFrom({0x1100, chrome_module.get()}));
  EXPECT_TRUE(unwinder.CanUnwindFrom({0x1000, chrome_module.get()}));
  EXPECT_FALSE(unwinder.CanUnwindFrom({0x2100, non_chrome_module.get()}));
  EXPECT_FALSE(unwinder.CanUnwindFrom({0x400, nullptr}));
}

namespace {
void ExpectFramesEq(const std::vector<Frame>& expected,
                    const std::vector<Frame>& actual) {
  EXPECT_EQ(actual.size(), expected.size());
  if (actual.size() != expected.size())
    return;

  for (size_t i = 0; i < actual.size(); i++) {
    EXPECT_EQ(expected[i].module, actual[i].module);
    EXPECT_EQ(expected[i].instruction_pointer, actual[i].instruction_pointer);
  }
}

class AlignedStackMemory {
 public:
  AlignedStackMemory(std::initializer_list<uintptr_t> values)
      : size_(values.size()),
        stack_memory_(static_cast<uintptr_t*>(
            AlignedAlloc(size_ * sizeof(uintptr_t), 2 * sizeof(uintptr_t)))) {
    DCHECK_EQ(size_ % 2, 0u);
    ranges::copy(values, stack_memory_.get());
  }

  uintptr_t stack_start_address() const {
    return reinterpret_cast<uintptr_t>(stack_memory_.get());
  }

  uintptr_t stack_end_address() const {
    return reinterpret_cast<uintptr_t>(stack_memory_.get() + size_);
  }

 private:
  const uintptr_t size_;
  const std::unique_ptr<uintptr_t, AlignedFreeDeleter> stack_memory_;
};

}  // namespace

TEST(ChromeUnwinderAndroidTest, TryUnwind) {
  const uint32_t page_table[] = {0, 2};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0, 0},     // Function 0.
      {0x10, 4},  // Function 1. The function to unwind 2 times.
      // Page 1.
      {0x5, 8},    // Function 2.
      {0x20, 12},  // Function 3.
  };
  const uint8_t function_offset_table[] = {
      // Function 0.
      0x2,
      0,
      0x0,
      1,
      // Function 1.
      0x7f,
      0,
      0x0,
      1,
      // Function 2.
      0x78,
      0,
      0x0,
      1,
      // Function 3.
      0x2,
      0,
      0x0,
      1,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: Pop r4, r14 from stack top.
      // Need to pop 2 registers to keep SP aligned.
      0b10101000,
      // Offset 1: COMPLETE.
      0b10110000,
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);

  // Both first_pc and second_pc lie in Function 1's address range.
  uintptr_t first_pc = text_section_start_address + 0x20;
  uintptr_t second_pc = text_section_start_address + page_size + 0x4;
  // third_pc lies outside chrome_module's address range.
  uintptr_t third_pc = text_section_start_address + 3 * page_size;

  AlignedStackMemory stack_memory = {
      0x0,
      third_pc,
      0xFFFF,
      0xFFFF,
  };

  std::vector<Frame> unwound_frames = {{first_pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = first_pc;
  RegisterContextStackPointer(&context) = stack_memory.stack_start_address();
  context.arm_lr = second_pc;

  EXPECT_EQ(
      UnwindResult::kUnrecognizedFrame,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>({{first_pc, chrome_module},
                                     {second_pc, chrome_module},
                                     {third_pc, nullptr}}),
                 unwound_frames);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindInfiniteLoopSingleFrame) {
  const uint32_t page_table[] = {0, 2};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0x0, 0},   // Refuse to unwind filler function.
      {0x10, 2},  // Function 0. The function to unwind.
      // Page 1.
      {0x5, 0},  // Refuse to unwind filler function.
  };
  const uint8_t function_offset_table[] = {
      // Refuse to unwind filler function.
      0x0,
      0,
      // Function 0.
      0x0,
      2,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: REFUSE_TO_UNWIND.
      0b10000000,
      0b00000000,
      // Offset 2: COMPLETE.
      0b10110000,
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);
  uintptr_t pc = text_section_start_address + 0x20;

  AlignedStackMemory stack_memory = {
      0xFFFF,
      0xFFFF,
  };

  std::vector<Frame> unwound_frames = {{pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = pc;
  RegisterContextStackPointer(&context) = stack_memory.stack_start_address();

  // Set lr = pc so that both sp and pc stays the same after first round of
  // unwind.
  context.arm_lr = pc;

  EXPECT_EQ(
      UnwindResult::kAborted,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>({{pc, chrome_module}}), unwound_frames);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindInfiniteLoopMultipleFrames) {
  // This test aims to produce a scenario, where after the unwind of a number
  // of frames, the sp and pc get to their original state before the unwind.

  // Function 1 (pc1, sp1):
  // - set pc = lr(pc2)
  // Function 2 (pc2, sp1):
  // - pop r14(pc2), r15(pc1) off stack
  // - vsp = r4 (reset vsp to frame initial vsp)

  const uint32_t page_table[] = {0, 3};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0x0, 0},    // Refuse to unwind filler function.
      {0x10, 2},   // Function 1. The function to unwind.
      {0x100, 2},  // Function 2. The function to unwind.
      // Page 1.
      {0x5, 0},  // Refuse to unwind filler function.
  };
  const uint8_t function_offset_table[] = {
      // Refuse to unwind filler function.
      0x0,
      0,
      // Function 0.
      0x0,
      2,
      // Function 1.
      0x2,
      3,
      0x1,
      5,
      0x0,
      6,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: REFUSE_TO_UNWIND.
      0b10000000,
      0b00000000,
      // Offset 2: COMPLETE.
      0b10110000,
      // Offset 3: POP r14, r15 off the stack.
      0b10001100,
      0b00000000,
      // Offset 5: vsp = r4.
      0b10010100,
      // Offset 6: COMPLETE.
      0b10110000,
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);
  uintptr_t first_pc = text_section_start_address + 0x20;    // Function 1.
  uintptr_t second_pc = text_section_start_address + 0x110;  // Function 2.

  AlignedStackMemory stack_memory = {
      second_pc,
      first_pc,
      0xFFFF,
      0xFFFF,
  };

  std::vector<Frame> unwound_frames = {{first_pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = first_pc;
  RegisterContextStackPointer(&context) = stack_memory.stack_start_address();

  context.arm_lr = second_pc;
  context.arm_r4 = stack_memory.stack_start_address();

  EXPECT_EQ(
      UnwindResult::kAborted,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>(
                     {{first_pc, chrome_module}, {second_pc, chrome_module}}),
                 unwound_frames);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindUnalignedSPFrameUnwind) {
  // SP should be 2-uintptr_t aligned before/after each frame unwind.
  const uint32_t page_table[] = {0, 2};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0x0, 0},   // Refuse to unwind filler function.
      {0x10, 2},  // Function 0. The function to unwind.
      // Page 1.
      {0x5, 0},  // Refuse to unwind filler function.
  };
  const uint8_t function_offset_table[] = {
      // Refuse to unwind filler function.
      0x0,
      0,
      // Function 0.
      0x0,
      2,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: REFUSE_TO_UNWIND.
      0b10000000,
      0b00000000,
      // Offset 2: COMPLETE.
      0b10110000,
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);
  uintptr_t pc = text_section_start_address + 0x20;

  AlignedStackMemory stack_memory = {
      0xFFFF,
      0xFFFF,
  };

  std::vector<Frame> unwound_frames = {{pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = pc;
  // Make stack memory not aligned to 2 * sizeof(uintptr_t);
  RegisterContextStackPointer(&context) =
      stack_memory.stack_start_address() + sizeof(uintptr_t);

  // The address is outside chrome module, which will result the unwind to
  // stop with result kUnrecognizedFrame if SP alignment issue was not detected.
  context.arm_lr =
      text_section_start_address + (number_of_pages + 1) * page_size;

  EXPECT_EQ(
      UnwindResult::kAborted,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>({{pc, chrome_module}}), unwound_frames);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindUnalignedSPInstructionUnwind) {
  // SP should be uintptr_t aligned before/after each unwind instruction
  // execution.

  const uint32_t page_table[] = {0, 2};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0x0, 0},   // Refuse to unwind filler function.
      {0x10, 2},  // Function 0. The function to unwind.
      // Page 1.
      {0x5, 0},  // Refuse to unwind filler function.
  };
  const uint8_t function_offset_table[] = {
      // Refuse to unwind filler function.
      0x0,
      0,
      // Function 0.
      0x0,
      2,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: REFUSE_TO_UNWIND.
      0b10000000, 0b00000000,
      // Offset 2:
      0b10010100,  // vsp = r4, where r4 = stack + (sizeof(uintptr_t) / 2)
      0b10110000,  // COMPLETE.
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);
  uintptr_t pc = text_section_start_address + 0x20;

  AlignedStackMemory stack_memory = {
      0xFFFF,
      0xFFFF,
  };

  std::vector<Frame> unwound_frames = {{pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = pc;
  RegisterContextStackPointer(&context) = stack_memory.stack_start_address();

  // The address is outside chrome module, which will result the unwind to
  // stop with result kUnrecognizedFrame if SP alignment issue was not detected.
  context.arm_lr =
      text_section_start_address + (number_of_pages + 1) * page_size;

  context.arm_r4 = stack_memory.stack_start_address() + sizeof(uintptr_t) / 2;

  EXPECT_EQ(
      UnwindResult::kAborted,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>({{pc, chrome_module}}), unwound_frames);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindSPOverflow) {
  const uint32_t page_table[] = {0, 2};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0x0, 0},   // Refuse to unwind filler function.
      {0x10, 2},  // Function 0. The function to unwind.
      // Page 1.
      {0x5, 0},  // Refuse to unwind filler function.
  };
  const uint8_t function_offset_table[] = {
      // Refuse to unwind filler function.
      0x0,
      0,
      // Function 0.
      0x0,
      2,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: REFUSE_TO_UNWIND.
      0b10000000, 0b00000000,
      // Offset 2.
      0b10010100,  // vsp = r4.
      0b10101000,  // Pop r4, r14.
      0b10110000,  // COMPLETE.
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);
  uintptr_t pc = text_section_start_address + 0x20;

  AlignedStackMemory stack_memory = {
      0xFFFF,
      0xFFFF,
  };
  std::vector<Frame> unwound_frames = {{pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = pc;
  RegisterContextStackPointer(&context) = stack_memory.stack_start_address();

  // Setting vsp = 0xffffffff should cause SP overflow.
  context.arm_r4 = 0xffffffff;

  // The address is outside chrome module, which will result the unwind to
  // stop with result kUnrecognizedFrame if the unwinder did not abort for other
  // reasons.
  context.arm_lr =
      text_section_start_address + (number_of_pages + 1) * page_size;

  EXPECT_EQ(
      UnwindResult::kAborted,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>({{pc, chrome_module}}), unwound_frames);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindNullSP) {
  const uint32_t page_table[] = {0, 2};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0x0, 0},   // Refuse to unwind filler function.
      {0x10, 2},  // Function 0. The function to unwind.
      // Page 1.
      {0x5, 0},  // Refuse to unwind filler function.
  };
  const uint8_t function_offset_table[] = {
      // Refuse to unwind filler function.
      0x0,
      0,
      // Function 0.
      0x0,
      2,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: REFUSE_TO_UNWIND.
      0b10000000, 0b00000000,
      // Offset 2.
      0b10010100,  // vsp = r4.
      0b10101000,  // Pop r4, r14.
      0b10110000,  // COMPLETE.
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);
  uintptr_t pc = text_section_start_address + 0x20;

  AlignedStackMemory stack_memory = {
      0xFFFF,
      0xFFFF,
  };
  std::vector<Frame> unwound_frames = {{pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = pc;
  RegisterContextStackPointer(&context) = stack_memory.stack_start_address();

  // Setting vsp = 0x0 should cause the unwinder to abort.
  context.arm_r4 = 0x0;

  // The address is outside chrome module, which will result the unwind to
  // stop with result kUnrecognizedFrame if the unwinder did not abort for other
  // reasons.
  context.arm_lr =
      text_section_start_address + (number_of_pages + 1) * page_size;

  EXPECT_EQ(
      UnwindResult::kAborted,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>({{pc, chrome_module}}), unwound_frames);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindInvalidSPOperation) {
  // This test aims to verify that for each unwind instruction executed, it is
  // always true that sp > frame initial sp.

  const uint32_t page_table[] = {0, 2};
  const size_t number_of_pages = std::size(page_table);
  const size_t page_size = 1 << 17;

  const FunctionTableEntry function_table[] = {
      // Page 0.
      {0x0, 0},   // Refuse to unwind filler function.
      {0x10, 2},  // Function 0. The function to unwind.
      // Page 1.
      {0x5, 0},  // Refuse to unwind filler function.
  };
  const uint8_t function_offset_table[] = {
      // Refuse to unwind filler function.
      0x0,
      0,
      // Function 0.
      0x0,
      2,
  };
  const uint8_t unwind_instruction_table[] = {
      // Offset 0: REFUSE_TO_UNWIND.
      0b10000000, 0b00000000,
      // Offset 2.
      0b10010100,  // vsp = r4 (r4 < frame initial sp).
      0b10010101,  // vsp = r5 (r5 > frame initial sp).
      0b10110000,  // COMPLETE.
  };

  auto unwind_info = ChromeUnwindInfoAndroid{
      unwind_instruction_table,
      function_offset_table,
      function_table,
      page_table,
  };

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(
                         0x1000, number_of_pages * page_size, "ChromeModule"));

  uintptr_t text_section_start_address = 0x1100;
  ChromeUnwinderAndroid unwinder(unwind_info, chrome_module->GetBaseAddress(),
                                 text_section_start_address);

  unwinder.Initialize(&module_cache);
  uintptr_t pc = text_section_start_address + 0x20;

  AlignedStackMemory stack_memory = {
      0xFFFF,
      0xFFFF,
  };
  std::vector<Frame> unwound_frames = {{pc, chrome_module}};
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = pc;
  RegisterContextStackPointer(&context) = stack_memory.stack_start_address();

  context.arm_r4 = stack_memory.stack_start_address() - 2 * sizeof(uintptr_t);
  context.arm_r5 = stack_memory.stack_start_address() + 2 * sizeof(uintptr_t);

  // The address is outside chrome module, which will result the unwind to
  // stop with result kUnrecognizedFrame if the unwinder did not abort for other
  // reasons.
  context.arm_lr =
      text_section_start_address + (number_of_pages + 1) * page_size;

  EXPECT_EQ(
      UnwindResult::kAborted,
      unwinder.TryUnwind(/*capture_state=*/nullptr, &context,
                         stack_memory.stack_end_address(), &unwound_frames));
  ExpectFramesEq(std::vector<Frame>({{pc, chrome_module}}), unwound_frames);
}

}  // namespace base
