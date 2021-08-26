// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/chrome_unwind_table_android.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"

namespace base {
namespace {

uintptr_t* GetRegisterPointer(RegisterContext* context,
                              uint8_t register_index) {
  DCHECK_LE(register_index, 14);
  static unsigned long RegisterContext::*const registers[15] = {
      &RegisterContext::arm_r0,  &RegisterContext::arm_r1,
      &RegisterContext::arm_r2,  &RegisterContext::arm_r3,
      &RegisterContext::arm_r4,  &RegisterContext::arm_r5,
      &RegisterContext::arm_r6,  &RegisterContext::arm_r7,
      &RegisterContext::arm_r8,  &RegisterContext::arm_r9,
      &RegisterContext::arm_r10, &RegisterContext::arm_fp,
      &RegisterContext::arm_ip,  &RegisterContext::arm_sp,
      &RegisterContext::arm_lr,
  };
  return reinterpret_cast<uintptr_t*>(&(context->*registers[register_index]));
}

// Pops the value on the top of stack out and assign it to target register.
// This is equivalent to arm instruction `Pop r[n]` where n = `register_index`.
// Returns whether the pop is successful.
bool PopRegister(RegisterContext* context, uint8_t register_index) {
  const uintptr_t sp = RegisterContextStackPointer(context);
  const uintptr_t stacktop_value = *reinterpret_cast<uintptr_t*>(sp);
  const auto new_sp = CheckedNumeric<uintptr_t>(sp) + sizeof(uintptr_t);
  const bool success =
      new_sp.AssignIfValid(&RegisterContextStackPointer(context));
  if (success)
    *GetRegisterPointer(context, register_index) = stacktop_value;
  return success;
}

// Decodes the given bytes as an ULEB128 format number and advances the bytes
// pointer by the size of ULEB128.
//
// This function assumes the given bytes are in valid ULEB128
// format and the decoded number should not overflow `uintptr_t` type.
uintptr_t DecodeULEB128(const uint8_t*& bytes) {
  uintptr_t value = 0;
  unsigned shift = 0;
  do {
    DCHECK_LE(shift, sizeof(uintptr_t) * 8) << "ULEB128 must not overflow.";
    value += (*bytes & 0x7f) << shift;
    shift += 7;
  } while (*bytes++ & 0x80);
  return value;
}

uint8_t GetTopBits(uint8_t byte, unsigned bits) {
  DCHECK_LE(bits, 8u);
  return byte >> (8 - bits);
}

}  // namespace

UnwindInstructionResult ExecuteUnwindInstruction(
    const uint8_t*& instruction,
    RegisterContext* thread_context) {
  if (GetTopBits(*instruction, 2) == 0b00) {
    // 00xxxxxx
    // vsp = vsp + (xxxxxx << 2) + 4. Covers range 0x04-0x100 inclusive.
    const uintptr_t offset = ((*instruction++ & 0b00111111) << 2) + 4;

    const auto new_sp =
        CheckedNumeric<uintptr_t>(RegisterContextStackPointer(thread_context)) +
        offset;
    if (!new_sp.AssignIfValid(&RegisterContextStackPointer(thread_context))) {
      return UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS;
    }
  } else if (GetTopBits(*instruction, 2) == 0b01) {
    // 01xxxxxx
    // vsp = vsp - (xxxxxx << 2) - 4. Covers range 0x04-0x100 inclusive.
    const uintptr_t offset = ((*instruction++ & 0b00111111) << 2) + 4;
    const auto new_sp =
        CheckedNumeric<uintptr_t>(RegisterContextStackPointer(thread_context)) -
        offset;
    if (!new_sp.AssignIfValid(&RegisterContextStackPointer(thread_context))) {
      return UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS;
    }
  } else if (GetTopBits(*instruction, 4) == 0b1001) {
    // 1001nnnn (nnnn != 13,15)
    // Set vsp = r[nnnn].
    const uint8_t register_index = *instruction++ & 0b00001111;
    DCHECK_NE(register_index, 13) << "Must not set sp to sp.";
    DCHECK_NE(register_index, 15) << "Must not set sp to pc.";
    // Note: We shouldn't have cases that are setting caller-saved registers
    // using this instruction.
    DCHECK_GE(register_index, 4)
        << "Must not set sp to caller-saved registers.";

    RegisterContextStackPointer(thread_context) =
        *GetRegisterPointer(thread_context, register_index);
  } else if (GetTopBits(*instruction, 5) == 0b10101) {
    // 10101nnn
    // Pop r4-r[4+nnn], r14
    const uint8_t max_register_index = (*instruction++ & 0b00000111) + 4;
    bool success = true;
    for (uint8_t n = 4; n <= max_register_index && success; n++) {
      success = PopRegister(thread_context, n);
    }
    if (success)
      success = PopRegister(thread_context, 14);
    if (!success)
      return UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS;
  } else if (*instruction == 0b10110000) {
    // Finish
    // Code 0xb0, Finish, copies VRS[r14] to VRS[r15] and also
    // indicates that no further instructions are to be processed for this
    // frame.
    // Note: As we are not supporting any instructions that can set r15 (pc),
    // we always need to copy r14 (lr) to r15 (pc).
    instruction++;
    thread_context->arm_pc = thread_context->arm_lr;
    return UnwindInstructionResult::COMPLETED;
  } else if (*instruction == 0b10110010) {
    // 10110010 uleb128
    // vsp = vsp + 0x204 + (uleb128 << 2)
    // (for vsp increments of 0x104-0x200, use 00xxxxxx twice)
    instruction++;
    const auto new_sp =
        CheckedNumeric<uintptr_t>(RegisterContextStackPointer(thread_context)) +
        (CheckedNumeric<uintptr_t>(DecodeULEB128(instruction)) << 2) + 0x204;

    if (!new_sp.AssignIfValid(&RegisterContextStackPointer(thread_context))) {
      return UnwindInstructionResult::STACK_POINTER_OUT_OF_BOUNDS;
    }
  } else {
    NOTREACHED();
  }
  return UnwindInstructionResult::INSTRUCTION_PENDING;
}

}  // namespace base