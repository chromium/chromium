// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/chrome_unwind_table_android.h"

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

TEST(ChromeUnwindTableAndroidTest,
     TestFunctionOffsetTableLookupExactMatchingOffset) {
  const uint8_t unwind_instruction_table[] = {0, 1, 2, 3, 4, 5, 6};
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

  EXPECT_EQ(unwind_instruction_table + 3,
            GetFirstUnwindInstructionFromFunctionOffsetTableIndex(
                unwind_instruction_table, function_offset_table,
                {/* instruction_offset_from_function_start */ 128,
                 /* function_offset_table_byte_index */ 0x0}));
}

TEST(ChromeUnwindTableAndroidTest,
     TestFunctionOffsetTableLookupNonExactMatchingOffset) {
  const uint8_t unwind_instruction_table[] = {0, 1, 2, 3, 4, 5, 6};
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

  EXPECT_EQ(unwind_instruction_table + 3,
            GetFirstUnwindInstructionFromFunctionOffsetTableIndex(
                unwind_instruction_table, function_offset_table,
                {/* instruction_offset_from_function_start */ 129,
                 /* function_offset_table_byte_index */ 0x0}));
}

TEST(ChromeUnwindTableAndroidTest, TestFunctionOffsetTableLookupZeroOffset) {
  const uint8_t unwind_instruction_table[] = {0, 1, 2, 3, 4, 5, 6};
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

  EXPECT_EQ(unwind_instruction_table + 4,
            GetFirstUnwindInstructionFromFunctionOffsetTableIndex(
                unwind_instruction_table, function_offset_table,
                {/* instruction_offset_from_function_start */ 0,
                 /* function_offset_table_byte_index */ 0x0}));
}

TEST(ChromeUnwindTableAndroidTest, TestAddressTableLookupEntryInPage) {
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
    ASSERT_NE(absl::nullopt, entry_found);
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
    ASSERT_NE(absl::nullopt, entry_found);
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
    ASSERT_NE(absl::nullopt, entry_found);
    // 0xffff - 6 = 0xfff9.
    EXPECT_EQ(0xfff9, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(70ul, entry_found->function_offset_table_byte_index);
  }
}

TEST(ChromeUnwindTableAndroidTest, TestAddressTableLookupEmptyPage) {
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
  ASSERT_NE(absl::nullopt, entry_found);
  EXPECT_EQ(0x10004, entry_found->instruction_offset_from_function_start);
  EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
}

TEST(ChromeUnwindTableAndroidTest,
     TestAddressTableLookupInvalidIntructionOffset) {
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
    ASSERT_EQ(absl::nullopt, entry_found);
  }
  {
    const uint32_t page_number = 2;
    const uint32_t page_instruction_offset = 0;
    const auto entry_found = GetFunctionTableIndexFromInstructionOffset(
        page_start_instructions, function_offset_table_indices,
        /* instruction_offset */ (page_instruction_offset << 1) +
            (page_number << 17));
    ASSERT_EQ(absl::nullopt, entry_found);
  }
}

TEST(ChromeUnwindTableAndroidTest,
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
  ASSERT_NE(absl::nullopt, entry_found);
  EXPECT_EQ(0x10004, entry_found->instruction_offset_from_function_start);
  EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
}

TEST(ChromeUnwindTableAndroidTest,
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
    ASSERT_NE(absl::nullopt, entry_found);
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
    ASSERT_NE(absl::nullopt, entry_found);
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
    ASSERT_NE(absl::nullopt, entry_found);
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
    ASSERT_NE(absl::nullopt, entry_found);
    EXPECT_EQ(0x30004, entry_found->instruction_offset_from_function_start);
    EXPECT_EQ(20ul, entry_found->function_offset_table_byte_index);
  }
}
}  // namespace base
