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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x10000014ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerIncrementMaxValue) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b00111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x10000100ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerIncrementOverflow) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b00111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0xffffffff;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0xffffffff, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerDecrementMinValue) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b01000000;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x0fffffecul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerDecrementMaxValue) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b01111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x10000000;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(0x0fffff00ul, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest,
     TestSmallStackPointerDecrementUnderflow) {
  RegisterContext thread_context = {};
  const uint8_t instruction = 0b01111111;
  const uint8_t* current_instruction = &instruction;
  thread_context.arm_sp = 0x00000000;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(100ul + register_index, thread_context.arm_sp);
}

TEST(ChromeAndroidUnwindInstructionTest, TestFinish) {
  RegisterContext thread_context = {};
  thread_context.arm_lr = 114;  // r14
  thread_context.arm_pc = 115;  // r15

  const uint8_t instruction = 0b10110000;
  const uint8_t* current_instruction = &instruction;
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::COMPLETED);
  ASSERT_EQ(current_instruction, &instruction + 1);
  EXPECT_EQ(114ul, thread_context.arm_pc);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::INSTRUCTION_PENDING);
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
  ASSERT_EQ(ExecuteUnwindInstruction(current_instruction, &thread_context),
            UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS);
  ASSERT_EQ(current_instruction,
            increment_overflow + sizeof(increment_overflow));
  EXPECT_EQ(0xfffffffful, thread_context.arm_sp);
}

}  // namespace base
