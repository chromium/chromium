// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/chrome_unwinder_android.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/memory/aligned_memory.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/profiler/chrome_unwind_info_android.h"

namespace base {
namespace {

uintptr_t* GetRegisterPointer(RegisterContext* context,
                              uint8_t register_index) {
  DCHECK_LE(register_index, 15);
  static unsigned long RegisterContext::*const registers[16] = {
      &RegisterContext::arm_r0,  &RegisterContext::arm_r1,
      &RegisterContext::arm_r2,  &RegisterContext::arm_r3,
      &RegisterContext::arm_r4,  &RegisterContext::arm_r5,
      &RegisterContext::arm_r6,  &RegisterContext::arm_r7,
      &RegisterContext::arm_r8,  &RegisterContext::arm_r9,
      &RegisterContext::arm_r10, &RegisterContext::arm_fp,
      &RegisterContext::arm_ip,  &RegisterContext::arm_sp,
      &RegisterContext::arm_lr,  &RegisterContext::arm_pc,
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
    DCHECK_LE(shift, sizeof(uintptr_t) * 8);  // ULEB128 must not overflow.
    value += (*bytes & 0x7fu) << shift;
    shift += 7;
  } while (*bytes++ & 0x80);
  return value;
}

uint8_t GetTopBits(uint8_t byte, unsigned bits) {
  DCHECK_LE(bits, 8u);
  return byte >> (8 - bits);
}

}  // namespace

ChromeUnwinderAndroid::ChromeUnwinderAndroid(
    const ChromeUnwindInfoAndroid& unwind_info,
    uintptr_t chrome_module_base_address,
    uintptr_t text_section_start_address)
    : unwind_info_(unwind_info),
      chrome_module_base_address_(chrome_module_base_address),
      text_section_start_address_(text_section_start_address) {
  DCHECK_GT(text_section_start_address_, chrome_module_base_address_);
}

bool ChromeUnwinderAndroid::CanUnwindFrom(const Frame& current_frame) const {
  return current_frame.module &&
         current_frame.module->GetBaseAddress() == chrome_module_base_address_;
}

UnwindResult ChromeUnwinderAndroid::TryUnwind(
    UnwinderStateCapture* capture_state,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    std::vector<Frame>* stack) {
  DCHECK(CanUnwindFrom(stack->back()));
  uintptr_t frame_initial_sp = RegisterContextStackPointer(thread_context);
  const uintptr_t unwind_initial_pc =
      RegisterContextInstructionPointer(thread_context);

  do {
    const uintptr_t pc = RegisterContextInstructionPointer(thread_context);
    const uintptr_t instruction_byte_offset_from_text_section_start =
        pc - text_section_start_address_;

    const std::optional<FunctionOffsetTableIndex> function_offset_table_index =
        GetFunctionTableIndexFromInstructionOffset(
            unwind_info_.page_table, unwind_info_.function_table,
            instruction_byte_offset_from_text_section_start);

    if (!function_offset_table_index) {
      return UnwindResult::kAborted;
    }

    const uint32_t current_unwind_instruction_index =
        GetFirstUnwindInstructionIndexFromFunctionOffsetTableEntry(
            &unwind_info_
                 .function_offset_table[function_offset_table_index
                                            ->function_offset_table_byte_index],
            function_offset_table_index
                ->instruction_offset_from_function_start);

    const uint8_t* current_unwind_instruction =
        &unwind_info_
             .unwind_instruction_table[current_unwind_instruction_index];

    UnwindInstructionResult instruction_result;
    bool pc_was_updated = false;

    do {
      instruction_result = ExecuteUnwindInstruction(
          current_unwind_instruction, pc_was_updated, thread_context);
      const uintptr_t sp = RegisterContextStackPointer(thread_context);
      if (sp > stack_top || sp < frame_initial_sp ||
          !IsAligned(sp, sizeof(uintptr_t))) {
        return UnwindResult::kAborted;
      }
    } while (instruction_result ==
             UnwindInstructionResult::kInstructionPending);

    if (instruction_result == UnwindInstructionResult::kAborted) {
      return UnwindResult::kAborted;
    }

    DCHECK_EQ(instruction_result, UnwindInstructionResult::kCompleted);

    const uintptr_t new_sp = RegisterContextStackPointer(thread_context);
    // Validate SP is properly aligned across frames.
    // See
    // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
    // for SP alignment rules.
    if (!IsAligned(new_sp, 2 * sizeof(uintptr_t))) {
      return UnwindResult::kAborted;
    }
    // Validate that SP does not decrease across frames.
    const bool is_leaf_frame = stack->size() == 1;
    // Each frame unwind is expected to only pop from stack memory, which will
    // cause sp to increase.
    // Non-Leaf frames are expected to at least pop lr off stack, so sp is
    // expected to strictly increase for non-leaf frames.
    if (new_sp <= (is_leaf_frame ? frame_initial_sp - 1 : frame_initial_sp)) {
      return UnwindResult::kAborted;
    }

    // For leaf functions, if SP does not change, PC must change, otherwise,
    // the overall execution state will be the same before/after the frame
    // unwind.
    if (is_leaf_frame && new_sp == frame_initial_sp &&
        RegisterContextInstructionPointer(thread_context) ==
            unwind_initial_pc) {
      return UnwindResult::kAborted;
    }

    frame_initial_sp = new_sp;

    stack->emplace_back(RegisterContextInstructionPointer(thread_context),
                        module_cache()->GetModuleForAddress(
                            RegisterContextInstructionPointer(thread_context)));
  } while (CanUnwindFrom(stack->back()));
  return UnwindResult::kUnrecognizedFrame;
}

UnwindInstructionResult ExecuteUnwindInstruction(
    const uint8_t*& instruction,
    bool& pc_was_updated,
    RegisterContext* thread_context) {
  if (GetTopBits(*instruction, 2) == 0b00) {
    // 00xxxxxx
    // vsp = vsp + (xxxxxx << 2) + 4. Covers range 0x04-0x100 inclusive.
    const uintptr_t offset = ((*instruction++ & 0b00111111u) << 2) + 4;

    const auto new_sp =
        CheckedNumeric<uintptr_t>(RegisterContextStackPointer(thread_context)) +
        offset;
    if (!new_sp.AssignIfValid(&RegisterContextStackPointer(thread_context))) {
      return UnwindInstructionResult::kAborted;
    }
  } else if (GetTopBits(*instruction, 2) == 0b01) {
    // 01xxxxxx
    // vsp = vsp - (xxxxxx << 2) - 4. Covers range 0x04-0x100 inclusive.
    const uintptr_t offset = ((*instruction++ & 0b00111111u) << 2) + 4;
    const auto new_sp =
        CheckedNumeric<uintptr_t>(RegisterContextStackPointer(thread_context)) -
        offset;
    if (!new_sp.AssignIfValid(&RegisterContextStackPointer(thread_context))) {
      return UnwindInstructionResult::kAborted;
    }
  } else if (GetTopBits(*instruction, 4) == 0b1001) {
    // 1001nnnn (nnnn != 13,15)
    // Set vsp = r[nnnn].
    const uint8_t register_index = *instruction++ & 0b00001111;
    DCHECK_NE(register_index, 13);  // Must not set sp to sp.
    DCHECK_NE(register_index, 15);  // Must not set sp to pc.
    // Note: We shouldn't have cases that are setting caller-saved registers
    // using this instruction.
    DCHECK_GE(register_index, 4);

    RegisterContextStackPointer(thread_context) =
        *GetRegisterPointer(thread_context, register_index);
  } else if (GetTopBits(*instruction, 5) == 0b10101) {
    // 10101nnn
    // Pop r4-r[4+nnn], r14
    const uint8_t max_register_index = (*instruction++ & 0b00000111u) + 4;
    for (uint8_t n = 4; n <= max_register_index; n++) {
      if (!PopRegister(thread_context, n)) {
        return UnwindInstructionResult::kAborted;
      }
    }
    if (!PopRegister(thread_context, 14)) {
      return UnwindInstructionResult::kAborted;
    }
  } else if (*instruction == 0b10000000 && *(instruction + 1) == 0) {
    // 10000000 00000000
    // Refuse to unwind.
    instruction += 2;
    return UnwindInstructionResult::kAborted;
  } else if (GetTopBits(*instruction, 4) == 0b1000) {
    const uint32_t register_bitmask =
        ((*instruction & 0xfu) << 8) + *(instruction + 1);
    instruction += 2;
    // 1000iiii iiiiiiii
    // Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}
    for (uint8_t register_index = 4; register_index < 16; register_index++) {
      if (register_bitmask & (1 << (register_index - 4))) {
        if (!PopRegister(thread_context, register_index)) {
          return UnwindInstructionResult::kAborted;
        }
      }
    }
    // If we set pc (r15) with value on stack, we should no longer copy lr to
    // pc on COMPLETE.
    pc_was_updated |= register_bitmask & (1 << (15 - 4));
  } else if (*instruction == 0b10110000) {
    // Finish
    // Code 0xb0, Finish, copies VRS[r14] to VRS[r15] and also
    // indicates that no further instructions are to be processed for this
    // frame.

    instruction++;
    // Only copy lr to pc when pc is not updated by other instructions before.
    if (!pc_was_updated)
      thread_context->arm_pc = thread_context->arm_lr;

    return UnwindInstructionResult::kCompleted;
  } else if (*instruction == 0b10110010) {
    // 10110010 uleb128
    // vsp = vsp + 0x204 + (uleb128 << 2)
    // (for vsp increments of 0x104-0x200, use 00xxxxxx twice)
    instruction++;
    const auto new_sp =
        CheckedNumeric<uintptr_t>(RegisterContextStackPointer(thread_context)) +
        (CheckedNumeric<uintptr_t>(DecodeULEB128(instruction)) << 2) + 0x204;

    if (!new_sp.AssignIfValid(&RegisterContextStackPointer(thread_context))) {
      return UnwindInstructionResult::kAborted;
    }
  } else {
    NOTREACHED();
  }
  return UnwindInstructionResult::kInstructionPending;
}

uintptr_t GetFirstUnwindInstructionIndexFromFunctionOffsetTableEntry(
    const uint8_t* function_offset_table_entry,
    int instruction_offset_from_function_start) {
  DCHECK_GE(instruction_offset_from_function_start, 0);
  const uint8_t* current_function_offset_table_position =
      function_offset_table_entry;

  do {
    const uintptr_t function_offset =
        DecodeULEB128(current_function_offset_table_position);

    const uintptr_t unwind_table_index =
        DecodeULEB128(current_function_offset_table_position);

    // Each function always ends at 0 offset. It is guaranteed to find an entry
    // as long as the function offset table is well-structured.
    if (function_offset <=
        static_cast<uint32_t>(instruction_offset_from_function_start))
      return unwind_table_index;

  } while (true);

  NOTREACHED();
}

const std::optional<FunctionOffsetTableIndex>
GetFunctionTableIndexFromInstructionOffset(
    span<const uint32_t> page_start_instructions,
    span<const FunctionTableEntry> function_offset_table_indices,
    uint32_t instruction_byte_offset_from_text_section_start) {
  DCHECK(!page_start_instructions.empty());
  DCHECK(!function_offset_table_indices.empty());
  // First function on first page should always start from 0 offset.
  DCHECK_EQ(function_offset_table_indices.front()
                .function_start_address_page_instruction_offset,
            0ul);

  const uint16_t page_number =
      instruction_byte_offset_from_text_section_start >> 17;
  const uint16_t page_instruction_offset =
      (instruction_byte_offset_from_text_section_start >> 1) &
      0xffff;  // 16 bits.

  // Invalid instruction_byte_offset_from_text_section_start:
  // instruction_byte_offset_from_text_section_start falls after the last page.
  if (page_number >= page_start_instructions.size()) {
    return std::nullopt;
  }

  const span<const FunctionTableEntry>::iterator function_table_entry_start =
      function_offset_table_indices.begin() +
      checked_cast<ptrdiff_t>(page_start_instructions[page_number]);
  const span<const FunctionTableEntry>::iterator function_table_entry_end =
      page_number == page_start_instructions.size() - 1
          ? function_offset_table_indices.end()
          : function_offset_table_indices.begin() +
                checked_cast<ptrdiff_t>(
                    page_start_instructions[page_number + 1]);

  // `std::upper_bound` finds first element that > target in range
  // [function_table_entry_start, function_table_entry_end).
  const auto first_larger_entry_location = std::upper_bound(
      function_table_entry_start, function_table_entry_end,
      page_instruction_offset,
      [](uint16_t page_instruction_offset, const FunctionTableEntry& entry) {
        return page_instruction_offset <
               entry.function_start_address_page_instruction_offset;
      });

  // Offsets the element found by 1 to get the biggest element that <= target.
  const auto entry_location = first_larger_entry_location - 1;

  // When all offsets in current range > page_instruction_offset (including when
  // there is no entry in current range), the `FunctionTableEntry` we are
  // looking for is not within the function_offset_table_indices range we are
  // inspecting, because the function is too long that it spans multiple pages.
  //
  // We need to locate the previous entry on function_offset_table_indices and
  // find its corresponding page_table index.
  //
  // Example:
  // +--------------------+--------------------+
  // | <-----2 byte-----> | <-----2 byte-----> |
  // +--------------------+--------------------+
  // | Page Offset        | Offset Table Index |
  // +--------------------+--------------------+-----
  // | 10                 | XXX                |  |
  // +--------------------+--------------------+  |
  // | ...                | ...                |Page 0x100
  // +--------------------+--------------------+  |
  // | 65500              | ZZZ                |  |
  // +--------------------+--------------------+----- Page 0x101 is empty
  // | 200                | AAA                |  |
  // +--------------------+--------------------+  |
  // | ...                | ...                |Page 0x102
  // +--------------------+--------------------+  |
  // | 65535              | BBB                |  |
  // +--------------------+--------------------+-----
  //
  // Example:
  // For
  // - page_number = 0x100, page_instruction_offset >= 65535
  // - page_number = 0x101, all page_instruction_offset
  // - page_number = 0x102, page_instruction_offset < 200
  // We should be able to map them all to entry [65500, ZZZ] in page 0x100.

  // Finds the page_number that corresponds to `entry_location`. The page
  // might not be the page we are inspecting, when the function spans over
  // multiple pages.
  uint16_t function_start_page_number = page_number;
  while (function_offset_table_indices.begin() +
             checked_cast<ptrdiff_t>(
                 page_start_instructions[function_start_page_number]) >
         entry_location) {
    // First page in page table must not be empty.
    DCHECK_NE(function_start_page_number, 0);
    function_start_page_number--;
  };

  const uint32_t function_start_address_instruction_offset =
      (uint32_t{function_start_page_number} << 16) +
      entry_location->function_start_address_page_instruction_offset;

  const int instruction_offset_from_function_start =
      static_cast<int>((instruction_byte_offset_from_text_section_start >> 1) -
                       function_start_address_instruction_offset);

  DCHECK_GE(instruction_offset_from_function_start, 0);
  return FunctionOffsetTableIndex{
      instruction_offset_from_function_start,
      entry_location->function_offset_table_byte_index,
  };
}

}  // namespace base
