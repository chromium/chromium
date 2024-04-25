// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/dwarf_line_no.h"

#include "partition_alloc/pointers/raw_ref.h"

#ifdef USE_SYMBOLIZE
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base/debug/buffered_dwarf_reader.h"
#include "base/third_party/symbolize/symbolize.h"
#include "partition_alloc/pointers/raw_ptr.h"

namespace base {
namespace debug {

namespace {

constexpr uint64_t kMaxOffset = std::numeric_limits<uint64_t>::max();

// These numbers are suitable for most compilation units for chrome and
// content_shell. If a compilation unit has bigger number of directories or
// filenames, the additional directories/filenames will be ignored, and the
// stack frames pointing to these directories/filenames will not get line
// numbers. We can't set these numbers too big because they affect the size of
// ProgramInfo which is allocated in the stack.
constexpr int kMaxDirectories = 128;
constexpr size_t kMaxFilenames = 512;

// DWARF-4 line number program header, section 6.2.4
struct ProgramInfo {
  uint64_t header_length;
  uint64_t start_offset;
  uint64_t end_offset;
  uint8_t minimum_instruction_length;
  uint8_t maximum_operations_per_instruction;
  uint8_t default_is_stmt;
  int8_t line_base;
  uint8_t line_range;
  uint8_t opcode_base;
  uint8_t standard_opcode_lengths[256];
  uint8_t include_directories_table_offset;
  uint8_t file_names_table_offset;

  // Store the directories as offsets.
  int num_directories = 1;
  uint64_t directory_offsets[kMaxDirectories];
  uint64_t directory_sizes[kMaxDirectories];

  // Store the file number table offsets.
  mutable unsigned int num_filenames = 1;
  mutable uint64_t filename_offsets[kMaxFilenames];
  mutable uint8_t filename_dirs[kMaxFilenames];

  unsigned int OpcodeToAdvance(uint8_t adjusted_opcode) const {
    // Special opcodes advance line numbers by an amount based on line_range
    // and opcode_base. This calculation is taken from 6.2.5.1.
    return static_cast<unsigned int>(adjusted_opcode) / line_range;
  }
};

// DWARF-4 line number program registers, section 6.2.2
struct LineNumberRegisters {
  // During the line number program evaluation, some instructions perform a
  // "commit" which is when the registers have finished calculating a new row in
  // the line-number table. This callback is executed and can be viewed as a
  // iterator over all rows in the line number table.
  class OnCommit {
   public:
    virtual void Do(LineNumberRegisters* registers) = 0;
  };

  raw_ptr<OnCommit> on_commit;
  LineNumberRegisters(ProgramInfo info, OnCommit* on_commit)
      : on_commit(on_commit), is_stmt(info.default_is_stmt) {}

  // Current program counter.
  uintptr_t address = 0;

  // For VLIW architectures, the index of the operation in the VLIW instruction.
  unsigned int op_index = 0;

  // Identifies the source file relating to the address in the DWARF File name
  // table.
  uint64_t file = 0;

  // Identifies the line number. Starts at 1. Can become 0 if instruction does
  // not match any line in the file.
  uint64_t line = 1;

  // Identifies the column within the source line. Starts at 1 though "0"
  // also means "left edge" of the line.
  uint64_t column = 0;

  // Boolean determining if this is a recommended spot for a breakpoint.
  // Should be initialized by the program header.
  bool is_stmt = false;

  // Indicates start of a basic block.
  bool basic_block = false;

  // Indicates first byte after a sequence of machine instructions.
  bool end_sequence = false;

  // Indicates this may be where execution should stop if trying to break for
  // entering a function.
  bool prologue_end = false;

  // Indicates this may be where execution should stop if trying to break for
  // exiting a function.
  bool epilogue_begin = false;

  // Identifier for the instruction set of the current address.
  uint64_t isa = 0;

  // Identifies which block the current instruction belongs to.
  uint64_t discriminator = 0;

  // Values from the previously committed line. See OnCommit interface for more
  // details. This conceptually should be a copy of the whole
  // LineNumberRegisters but since only 4 pieces of data are needed, hacking
  // it inline was easier.
  uintptr_t last_address = 0;
  uint64_t last_file = 0;
  uint64_t last_line = 0;
  uint64_t last_column = 0;

  // This is the magical calculation for decompressing the line-number
  // information. The `program_info` provides the parameters for the formula
  // and the `op_advance` is the input value. See DWARF-4 sections 6.2.5.1 for
  // the formula.
  void OpAdvance(const ProgramInfo* program_info, uint64_t op_advance) {
    address += program_info->minimum_instruction_length *
               ((op_index + op_advance) /
                program_info->maximum_operations_per_instruction);

    op_index = (op_index + op_advance) %
               program_info->maximum_operations_per_instruction;
  }

  // Committing a line means the calculation has landed on a stable set of
  // values that represent an actual entry in the line number table.
  void CommitLine() {
    on_commit->Do(this);

    // Inlined or compiler generator code may have line number 0 which isn't
    // useful to the user. Better to go up one line number.
    if (line != 0) {
      last_address = address;
      last_file = file;
      last_column = column;
      last_line = line;
    }
  }
};

struct LineNumberInfo {
  uint64_t pc = 0;
  uint64_t line = 0;
  uint64_t column = 0;

  // Offsets here are to the file table and directory table arrays inside the
  // ProgramInfo.
  uint64_t module_dir_offset = 0;
  uint64_t dir_size = 0;
  uint64_t module_filename_offset = 0;
};

// Evaluates a Line Number Program as defined by the rules in section 6.2.5.
void EvaluateLineNumberProgram(const int fd,
                               LineNumberInfo* info,
                               uint64_t base_address,
                               uint64_t start,
                               const ProgramInfo& program_info) {
  BufferedDwarfReader reader(fd, start);
  uint64_t module_relative_pc = info->pc - base_address;

  // Helper that records the line-number table entry corresponding with the
  // `module_relative_pc`. This is the thing that actually finds the line
  // number for an address.
  struct OnCommitImpl : public LineNumberRegisters::OnCommit {
   private:
    raw_ptr<LineNumberInfo> info;
    uint64_t module_relative_pc;
    const raw_ref<const ProgramInfo> program_info;

   public:
    OnCommitImpl(LineNumberInfo* info,
                 uint64_t module_relative_pc,
                 const ProgramInfo& program_info)
        : info(info),
          module_relative_pc(module_relative_pc),
          program_info(program_info) {}

    void Do(LineNumberRegisters* registers) override {
      // When a line is committed, the program counter needs to check if it is
      // in the [last_address, cur_addres) range. If yes, then the line pertains
      // to the program counter.
      if (registers->last_address == 0) {
        // This is the first table entry so by definition, nothing is in its
        // range.
        return;
      }

      // If module_relative_pc is out of range, skip.
      if (module_relative_pc < registers->last_address ||
          module_relative_pc >= registers->address)
        return;

      if (registers->last_file < program_info->num_filenames) {
        info->line = registers->last_line;
        info->column = registers->last_column;

        // Since DW_AT_name in the compile_unit is optional, it may be empty. If
        // it is, guess that the file in entry 1 is the name. This does not
        // follow spec, but seems to be common behavior. See the following LLVM
        // bug for more info: https://reviews.llvm.org/D11003
        if (registers->last_file == 0 &&
            program_info->filename_offsets[0] == 0 &&
            1 < program_info->num_filenames) {
          program_info->filename_offsets[0] = program_info->filename_offsets[1];
          program_info->filename_dirs[0] = program_info->filename_dirs[1];
        }

        if (registers->last_file < kMaxFilenames) {
          info->module_filename_offset =
              program_info->filename_offsets[registers->last_file];

          uint8_t dir = program_info->filename_dirs[registers->last_file];
          info->module_dir_offset = program_info->directory_offsets[dir];
          info->dir_size = program_info->directory_sizes[dir];
        }
      }
    }
  } on_commit(info, module_relative_pc, program_info);

  LineNumberRegisters registers(program_info, &on_commit);

  // Special opcode range is [program_info.opcode_base, 255].
  // Lines can be max incremented by [line_base + line range - 1].
  // opcode = (desired line increment - line_base) + (line_range * operation
  // advance) + opcode_base.
  uint8_t opcode;
  while (reader.position() < program_info.end_offset && info->line == 0) {
    if (!reader.ReadInt8(opcode))
      return;

    // It's SPECIAL OPCODE TIME!. They're so special that they make up the
    // vast majority of the opcodes and are the first thing described in the
    // documentation.
    //
    // See DWARF-4 spec 6.2.5.1.
    if (opcode >= program_info.opcode_base) {
      uint8_t adjusted_opcode = opcode - program_info.opcode_base;
      registers.OpAdvance(&program_info,
                          program_info.OpcodeToAdvance(adjusted_opcode));
      const int line_adjust =
          program_info.line_base + (adjusted_opcode % program_info.line_range);
      if (line_adjust < 0) {
        if (static_cast<uint64_t>(-line_adjust) > registers.line)
          return;
        registers.line -= static_cast<uint64_t>(-line_adjust);
      } else {
        registers.line += static_cast<uint64_t>(line_adjust);
      }
      registers.basic_block = false;
      registers.prologue_end = false;
      registers.epilogue_begin = false;
      registers.discriminator = 0;
      registers.CommitLine();
    } else {
      // Standard opcodes
      switch (opcode) {
        case 0: {
          // Extended opcode.
          uint64_t extended_opcode;
          uint64_t extended_opcode_length;
          if (!reader.ReadLeb128(extended_opcode_length))
            return;
          uint64_t next_opcode = reader.position() + extended_opcode_length;
          if (!reader.ReadLeb128(extended_opcode))
            return;
          switch (extended_opcode) {
            case 1: {
              // DW_LNE_end_sequence
              registers.end_sequence = true;
              registers.CommitLine();
              registers = LineNumberRegisters(program_info, &on_commit);
              break;
            }

            case 2: {
              // DW_LNE_set_address
              uint32_t value;
              if (!reader.ReadInt32(value))
                return;
              registers.address = value;
              registers.op_index = 0;
              break;
            }

            case 3: {
              // DW_LNE_define_file
              //
              // This should only get used if the filename table itself is null.
              // Record the module offset for the string and then drop the data.
              uint64_t filename_offset = reader.position();
              reader.ReadCString(program_info.end_offset, nullptr, 0);

              // dir index
              uint64_t value;
              if (!reader.ReadLeb128(value))
                return;
              size_t cur_filename = program_info.num_filenames;
              if (cur_filename < kMaxFilenames && value < kMaxDirectories) {
                ++program_info.num_filenames;
                // Store the offset from the start of file and skip the data to
                // save memory.
                program_info.filename_offsets[cur_filename] = filename_offset;
                program_info.filename_dirs[cur_filename] =
                    static_cast<uint8_t>(value);
              }

              // modification time
              if (!reader.ReadLeb128(value))
                return;

              // source file length
              if (!reader.ReadLeb128(value))
                return;
              break;
            }

            case 4: {
              // DW_LNE_set_discriminator
              uint64_t value;
              if (!reader.ReadLeb128(value))
                return;
              registers.discriminator = value;
              break;
            }

            default:
              abort();
          }

          // Skip any padding bytes in extended opcode.
          reader.set_position(next_opcode);
          break;
        }

        case 1: {
          // DW_LNS_copy. This commits the registers to the line number table.
          registers.CommitLine();
          registers.discriminator = 0;
          registers.basic_block = false;
          registers.prologue_end = false;
          registers.epilogue_begin = false;
          break;
        }

        case 2: {
          // DW_LNS_advance_pc
          uint64_t op_advance;
          if (!reader.ReadLeb128(op_advance))
            return;
          registers.OpAdvance(&program_info, op_advance);
          break;
        }

        case 3: {
          // DW_LNS_advance_line
          int64_t line_advance;
          if (!reader.ReadLeb128(line_advance))
            return;
          if (line_advance < 0) {
            if (static_cast<uint64_t>(-line_advance) > registers.line)
              return;
            registers.line -= static_cast<uint64_t>(-line_advance);
          } else {
            registers.line += static_cast<uint64_t>(line_advance);
          }
          break;
        }

        case 4: {
          // DW_LNS_set_file
          uint64_t value;
          if (!reader.ReadLeb128(value))
            return;
          registers.file = value;
          break;
        }

        case 5: {
          // DW_LNS_set_column
          uint64_t value;
          if (!reader.ReadLeb128(value))
            return;
          registers.column = value;
          break;
        }

        case 6:
          // DW_LNS_negate_stmt
          registers.is_stmt = !registers.is_stmt;
          break;

        case 7:
          // DW_LNS_set_basic_block
          registers.basic_block = true;
          break;

        case 8:
          // DW_LNS_const_add_pc
          registers.OpAdvance(
              &program_info,
              program_info.OpcodeToAdvance(255 - program_info.opcode_base));
          break;

        case 9: {
          // DW_LNS_fixed_advance_pc
          uint16_t value;
          if (!reader.ReadInt16(value))
            return;
          registers.address += value;
          registers.op_index = 0;
          break;
        }

        case 10:
          // DW_LNS_set_prologue_end
          registers.prologue_end = true;
          break;

        case 11:
          // DW_LNS_set_epilogue_begin
          registers.epilogue_begin = true;
          break;

        case 12: {
          // DW_LNS_set_isa
          uint64_t value;
          if (!reader.ReadLeb128(value))
            return;
          registers.isa = value;
          break;
        }

        default:
          abort();
      }
    }
  }
}

// Parses a 32-bit DWARF-4 line number program header per section 6.2.4.
// `cu_name_offset` is the module offset for the 0th entry of the file table.
bool ParseDwarf4ProgramInfo(BufferedDwarfReader* reader,
                            bool is_64bit,
                            uint64_t cu_name_offset,
                            ProgramInfo* program_info) {
  if (!reader->ReadOffset(is_64bit, program_info->header_length))
    return false;
  program_info->start_offset = reader->position() + program_info->header_length;

  if (!reader->ReadInt8(program_info->minimum_instruction_length) ||
      !reader->ReadInt8(program_info->maximum_operations_per_instruction) ||
      !reader->ReadInt8(program_info->default_is_stmt) ||
      !reader->ReadInt8(program_info->line_base) ||
      !reader->ReadInt8(program_info->line_range) ||
      !reader->ReadInt8(program_info->opcode_base)) {
    return false;
  }

  for (int i = 0; i < (program_info->opcode_base - 1); i++) {
    if (!reader->ReadInt8(program_info->standard_opcode_lengths[i]))
      return false;
  }

  // Table ends with a single null line. This basically means search for 2
  // contiguous empty bytes.
  uint8_t last = 0, cur = 0;
  for (;;) {
    // Read a byte.
    last = cur;
    if (!reader->ReadInt8(cur))
      return false;

    if (last == 0 && cur == 0) {
      // We're at the last entry where it's a double null.
      break;
    }

    // Read in all of the filename.
    int cur_dir = program_info->num_directories;
    if (cur_dir < kMaxDirectories) {
      ++program_info->num_directories;
      // "-1" is because we have already read the first byte above.
      program_info->directory_offsets[cur_dir] = reader->position() - 1;
      program_info->directory_sizes[cur_dir] = 1;
    }
    do {
      if (!reader->ReadInt8(cur))
        return false;
      if (cur_dir < kMaxDirectories)
        ++program_info->directory_sizes[cur_dir];
    } while (cur != '\0');
  }

  // Read filename table line-by-line.
  last = 0;
  cur = 0;
  for (;;) {
    // Read a byte.
    last = cur;
    if (!reader->ReadInt8(cur))
      return false;

    if (last == 0 && cur == 0) {
      // We're at the last entry where it's a double null.
      break;
    }

    // Read in all of the filename. "-1" is because we have already read the
    // first byte of the filename above.
    uint64_t filename_offset = reader->position() - 1;
    do {
      if (!reader->ReadInt8(cur))
        return false;
    } while (cur != '\0');

    uint64_t value;

    // Dir index
    if (!reader->ReadLeb128(value))
      return false;
    size_t cur_filename = program_info->num_filenames;
    if (cur_filename < kMaxFilenames && value < kMaxDirectories) {
      ++program_info->num_filenames;
      program_info->filename_offsets[cur_filename] = filename_offset;
      program_info->filename_dirs[cur_filename] = static_cast<uint8_t>(value);
    }

    // Modification time
    if (!reader->ReadLeb128(value))
      return false;

    // Bytes in file.
    if (!reader->ReadLeb128(value))
      return false;
  }

  // Set up the 0th filename.
  program_info->filename_offsets[0] = cu_name_offset;
  program_info->filename_dirs[0] = 0;
  program_info->directory_offsets[0] = 0;

  return true;
}

// Returns the offset of the next byte to read.
// `program_info.program_end` is guaranteed to be initlialized to either
// `kMaxOffset` if the program length could not be processed, or to
// the byte after the end of this program.
bool ReadProgramInfo(const int fd,
                     uint64_t start,
                     uint64_t cu_name_offset,
                     ProgramInfo* program_info) {
  BufferedDwarfReader reader(fd, start);
  program_info->end_offset = kMaxOffset;

  // Note that 64-bit dwarf does NOT imply a 64-bit binary and vice-versa. In
  // fact many 64-bit binaries use 32-bit dwarf encoding.
  bool is_64bit = false;
  uint64_t data_length;
  if (!reader.ReadInitialLength(is_64bit, data_length)) {
    return false;
  }

  // Set the program end. This allows the search to recover by skipping an
  // unparsable program.
  program_info->end_offset = reader.position() + data_length;

  uint16_t version;
  if (!reader.ReadInt16(version)) {
    return false;
  }

  if (version == 4) {
    return ParseDwarf4ProgramInfo(&reader, is_64bit, cu_name_offset,
                                  program_info);
  }

  // Currently does not support other DWARF versions.
  return false;
}

// Attempts to find line-number info for all of |info|. Returns the number of
// entries that do not have info yet.
uint64_t GetLineNumbersInProgram(const int fd,
                                 LineNumberInfo* info,
                                 uint64_t base_address,
                                 uint64_t start,
                                 uint64_t cu_name_offset) {
  // Open the program.
  ProgramInfo program_info;
  if (ReadProgramInfo(fd, start, cu_name_offset, &program_info)) {
    EvaluateLineNumberProgram(fd, info, base_address, program_info.start_offset,
                              program_info);
  }

  return program_info.end_offset;
}

// Scans the .debug_abbrev entry until it finds the Attribute List matching the
// `wanted_abbreviation_code`. This is called when parsing a DIE in .debug_info.
bool AdvancedReaderToAttributeList(BufferedDwarfReader& reader,
                                   uint64_t table_end,
                                   uint64_t wanted_abbreviation_code,
                                   uint64_t& tag,
                                   bool& has_children) {
  // Abbreviation Table entries are:
  //   LEB128 - abbreviation code
  //   LEB128 - the entry's tag
  //   1 byte - DW_CHILDREN_yes or DW_CHILDREN_no for if entry has children.
  //   [LEB128, LEB128] -- repeated set of attribute + form values in LEB128
  //   [0, 0] -- null entry terminating list is 2 LEB128 0s.
  while (reader.position() < table_end) {
    uint64_t abbreviation_code;
    if (!reader.ReadLeb128(abbreviation_code)) {
      return false;
    }

    if (!reader.ReadLeb128(tag)) {
      return false;
    }

    uint8_t raw_has_children;
    if (!reader.ReadInt8(raw_has_children)) {
      return false;
    }
    if (raw_has_children == 0) {
      has_children = false;
    } else if (raw_has_children == 1) {
      has_children = true;
    } else {
      return false;
    }

    if (abbreviation_code == wanted_abbreviation_code) {
      return true;
    }

    // Incorrect Abbreviation entry. Skip all of its attributes.
    uint64_t attr;
    uint64_t form;
    do {
      if (!reader.ReadLeb128(attr) || !reader.ReadLeb128(form)) {
        return false;
      }
    } while (attr != 0 || form != 0);
  }

  return false;
}

// This reads through a .debug_info compile unit entry to try and extract
// the `cu_name_offset` as well as the `debug_line_offset` (offset into the
// .debug_lines table` corresponding to `pc`.
//
// The .debug_info sections are a packed set of bytes whose format is defined
// by a corresponding .debug_abbrev entry. Basically .debug_abbrev describes
// a struct and .debug_info has a header that tells which struct it is followed
// by a bunch of bytes.
//
// The control flow is to find the .debug_abbrev entry for each .debug_info
// entry, then walk through the .debug_abbrev entry to parse the bytes of the
// .debug_info entry. A successful parse calculates the address range that the
// .debug_info entry covers. When that is retrieved, `pc` can be compared to
// the range and a corresponding .debug_info can be found.
//
// The `debug_info_start` be the start of the whole .debug_info section or an
// offset into the section if it was known ahead of time (perhaps by consulting
// .debug_aranges).
//
// To fully interpret this data, the .debug_ranges and .debug_str sections
// also need to be interpreted.
bool GetCompileUnitName(int fd,
                        uint64_t debug_info_start,
                        uint64_t debug_info_end,
                        uint64_t pc,
                        uint64_t module_base_address,
                        uint64_t* debug_line_offset,
                        uint64_t* cu_name_offset) {
  // Ensure defined `cu_name_offset` in case DW_AT_name is missing.
  *cu_name_offset = 0;

  // Open .debug_info and .debug_abbrev as both are needed to find the
  // DW_AT_name for the DW_TAG_compile_unit or DW_TAG_partial_unit
  // corresponding to the given address.

  ElfW(Shdr) debug_abbrev;
  constexpr static char kDebugAbbrevSectionName[] = ".debug_abbrev";
  if (!google::GetSectionHeaderByName(fd, kDebugAbbrevSectionName,
                                      sizeof(kDebugAbbrevSectionName),
                                      &debug_abbrev)) {
    return false;
  }
  uint64_t debug_abbrev_end = debug_abbrev.sh_offset + debug_abbrev.sh_size;

  ElfW(Shdr) debug_str;
  constexpr static char kDebugStrSectionName[] = ".debug_str";
  if (!google::GetSectionHeaderByName(
          fd, kDebugStrSectionName, sizeof(kDebugStrSectionName), &debug_str)) {
    return false;
  }
  uint64_t debug_str_end = debug_str.sh_offset + debug_str.sh_size;

  ElfW(Shdr) debug_ranges;
  constexpr static char kDebugRangesSectionName[] = ".debug_ranges";
  if (!google::GetSectionHeaderByName(fd, kDebugRangesSectionName,
                                      sizeof(kDebugRangesSectionName),
                                      &debug_ranges)) {
    return false;
  }
  uint64_t debug_ranges_end = debug_ranges.sh_offset + debug_ranges.sh_size;

  // Iterate Compile Units.
  uint64_t next_compilation_unit = kMaxOffset;
  for (BufferedDwarfReader reader(fd, debug_info_start);
       reader.position() < debug_info_end;
       reader.set_position(next_compilation_unit)) {
    bool is_64bit;
    uint64_t length;
    uint16_t dwarf_version;
    uint64_t abbrev_offset;
    uint8_t address_size;
    if (!reader.ReadCommonHeader(is_64bit, length, dwarf_version, abbrev_offset,
                                 address_size, next_compilation_unit)) {
      return false;
    }

    // Compilation Unit Header parsed. Now read the first tag which is either a
    // DW_TAG_compile_unit or DW_TAG_partial_unit. The entry type is designated
    // by a LEB128 number that needs to be cross-referenced in the abbreviations
    // table to understand the format of the rest of the entry.
    uint64_t abbreviation_code;
    if (!reader.ReadLeb128(abbreviation_code)) {
      return false;
    }

    // Find entry in the abbreviation table.
    BufferedDwarfReader abbrev_reader(fd,
                                      debug_abbrev.sh_offset + abbrev_offset);
    uint64_t tag;
    bool has_children;
    AdvancedReaderToAttributeList(abbrev_reader, debug_abbrev_end,
                                  abbreviation_code, tag, has_children);

    // Ignore if it has children.
    static constexpr int kDW_TAG_compile_unit = 0x11;
    static constexpr int kDW_TAG_partial_unit = 0x3c;
    if (tag != kDW_TAG_compile_unit && tag != kDW_TAG_partial_unit) {
      return false;
    }

    // Use table to parse the name, high, and low attributes.
    static constexpr int kDW_AT_name = 0x3;        // string
    static constexpr int kDW_AT_stmt_list = 0x10;  // lineptr
    static constexpr int kDW_AT_low_pc = 0x11;     // address
    static constexpr int kDW_AT_high_pc = 0x12;    // address, constant
    static constexpr int kDW_AT_ranges = 0x55;     // rangelistptr
    uint64_t attr;
    uint64_t form;
    uint64_t low_pc = 0;
    uint64_t high_pc = 0;
    bool high_pc_is_offset = false;
    bool is_found_in_range = false;
    do {
      if (!abbrev_reader.ReadLeb128(attr)) {
        return false;
      }
      if (!abbrev_reader.ReadLeb128(form)) {
        return false;
      }
      // Table from 7.5.4, Figure 20.
      enum Form {
        kDW_FORM_addr = 0x01,
        kDW_FORM_block2 = 0x03,
        kDW_FORM_block4 = 0x04,
        kDW_FORM_data2 = 0x05,
        kDW_FORM_data4 = 0x06,
        kDW_FORM_data8 = 0x07,
        kDW_FORM_string = 0x08,
        kDW_FORM_block = 0x09,
        kDW_FORM_block1 = 0x0a,
        kDW_FORM_data1 = 0x0b,
        kDW_FORM_flag = 0x0c,
        kDW_FORM_sdata = 0x0d,
        kDW_FORM_strp = 0x0e,
        kDW_FORM_udata = 0x0f,
        kDW_FORM_ref_addr = 0x10,
        kDW_FORM_ref1 = 0x11,
        kDW_FORM_ref2 = 0x12,
        kDW_FORM_ref4 = 0x13,
        kDW_FORM_ref8 = 0x14,
        kDW_FORM_ref_udata = 0x15,
        kDW_FORM_ref_indrect = 0x16,
        kDW_FORM_sec_offset = 0x17,
        kDW_FORM_exprloc = 0x18,
        kDW_FORM_flag_present = 0x19,
        kDW_FORM_ref_sig8 = 0x20,
      };

      switch (form) {
        case kDW_FORM_string: {
          // Read the value into if necessary `out`
          if (attr == kDW_AT_name) {
            *cu_name_offset = reader.position();
          }
          if (!reader.ReadCString(debug_info_end, nullptr, 0)) {
            return false;
          }
        } break;

        case kDW_FORM_strp: {
          uint64_t strp_offset;
          if (!reader.ReadOffset(is_64bit, strp_offset)) {
            return false;
          }

          if (attr == kDW_AT_name) {
            uint64_t pos = debug_str.sh_offset + strp_offset;
            if (pos >= debug_str_end) {
              return false;
            }
            *cu_name_offset = pos;
          }
        } break;

        case kDW_FORM_addr: {
          uint64_t address;
          if (!reader.ReadAddress(address_size, address)) {
            return false;
          }

          if (attr == kDW_AT_low_pc) {
            low_pc = address;
          } else if (attr == kDW_AT_high_pc) {
            high_pc_is_offset = false;
            high_pc = address;
          }
        } break;

        case kDW_FORM_data1: {
          uint8_t data;
          if (!reader.ReadInt8(data)) {
            return false;
          }
          if (attr == kDW_AT_high_pc) {
            high_pc_is_offset = true;
            high_pc = data;
          }
        } break;

        case kDW_FORM_data2: {
          uint16_t data;
          if (!reader.ReadInt16(data)) {
            return false;
          }
          if (attr == kDW_AT_high_pc) {
            high_pc_is_offset = true;
            high_pc = data;
          }
        } break;

        case kDW_FORM_data4: {
          uint32_t data;
          if (!reader.ReadInt32(data)) {
            return false;
          }
          if (attr == kDW_AT_high_pc) {
            high_pc_is_offset = true;
            high_pc = data;
          }
        } break;

        case kDW_FORM_data8: {
          uint64_t data;
          if (!reader.ReadInt64(data)) {
            return false;
          }
          if (attr == kDW_AT_high_pc) {
            high_pc_is_offset = true;
            high_pc = data;
          }
        } break;

        case kDW_FORM_sdata: {
          int64_t data;
          if (!reader.ReadLeb128(data)) {
            return false;
          }
          if (attr == kDW_AT_high_pc) {
            high_pc_is_offset = true;
            high_pc = static_cast<uint64_t>(data);
          }
        } break;

        case kDW_FORM_udata: {
          uint64_t data;
          if (!reader.ReadLeb128(data)) {
            return false;
          }
          if (attr == kDW_AT_high_pc) {
            high_pc_is_offset = true;
            high_pc = data;
          }
        } break;

        case kDW_FORM_ref_addr:
        case kDW_FORM_sec_offset: {
          uint64_t value;
          if (!reader.ReadOffset(is_64bit, value)) {
            return false;
          }

          if (attr == kDW_AT_ranges) {
            uint64_t current_base_address = module_base_address;
            BufferedDwarfReader ranges_reader(fd,
                                              debug_ranges.sh_offset + value);

            while (ranges_reader.position() < debug_ranges_end) {
              // Ranges are 2 addresses in size.
              uint64_t range_start;
              uint64_t range_end;
              if (!ranges_reader.ReadAddress(address_size, range_start)) {
                return false;
              }
              if (!ranges_reader.ReadAddress(address_size, range_end)) {
                return false;
              }
              uint64_t relative_pc = pc - current_base_address;

              if (range_start == 0 && range_end == 0) {
                if (!is_found_in_range) {
                  // Time to go to the next iteration.
                  goto next_cu;
                }
                break;
              } else if (((address_size == 4) &&
                          (range_start == 0xffffffffUL)) ||
                         ((address_size == 8) &&
                          (range_start == 0xffffffffffffffffULL))) {
                // Check if this is a new base add value. 2.17.3
                current_base_address = range_end;
              } else {
                if (relative_pc >= range_start && relative_pc < range_end) {
                  is_found_in_range = true;
                  break;
                }
              }
            }
          } else if (attr == kDW_AT_stmt_list) {
            *debug_line_offset = value;
          }
        } break;

        case kDW_FORM_flag:
        case kDW_FORM_ref1:
        case kDW_FORM_block1: {
          uint8_t dummy;
          if (!reader.ReadInt8(dummy)) {
            return false;
          }
        } break;

        case kDW_FORM_ref2:
        case kDW_FORM_block2: {
          uint16_t dummy;
          if (!reader.ReadInt16(dummy)) {
            return false;
          }
        } break;

        case kDW_FORM_ref4:
        case kDW_FORM_block4: {
          uint32_t dummy;
          if (!reader.ReadInt32(dummy)) {
            return false;
          }
        } break;

        case kDW_FORM_ref8: {
          uint64_t dummy;
          if (!reader.ReadInt64(dummy)) {
            return false;
          }
        } break;

        case kDW_FORM_ref_udata:
        case kDW_FORM_block: {
          uint64_t dummy;
          if (!reader.ReadLeb128(dummy)) {
            return false;
          }
        } break;

        case kDW_FORM_exprloc: {
          uint64_t value;
          if (!reader.ReadLeb128(value)) {
            return false;
          }
          reader.set_position(reader.position() + value);
        } break;
      }
    } while (attr != 0 || form != 0);

    // Because attributes can be in any order, most of the computations (minus
    // checking range list entries) cannot happen until everything is parsed for
    // the one .debug_info entry. Do the analysis here.
    if (is_found_in_range) {
      // Well formed compile_unit and partial_unit tags either have a
      // DT_AT_ranges entry or an DT_AT_low_pc entiry. If is_found_in_range
      // matched as true, then this entry matches the given pc.
      return true;
    }

    // If high_pc_is_offset is 0, it was never found in the DIE. This indicates
    // a single address entry. Only look at the low_pc.
    {
      uint64_t module_relative_pc = pc - module_base_address;
      if (high_pc == 0 && module_relative_pc != low_pc) {
        goto next_cu;
      }

      // Otherwise this is a contiguous range DIE. Normalize the meaning of the
      // high_pc field and check if it contains the pc.
      if (high_pc_is_offset) {
        high_pc = low_pc + high_pc;
        high_pc_is_offset = false;
      }

      if (module_relative_pc >= low_pc && module_relative_pc < high_pc) {
        return true;
      }
    }

    // Not found.
  next_cu:;
  }
  return false;
}

// Thin wrapper over `GetCompileUnitName` that opens the .debug_info section.
bool ReadCompileUnit(int fd,
                     uint64_t pc,
                     uint64_t cu_offset,
                     uint64_t base_address,
                     uint64_t* debug_line_offset,
                     uint64_t* cu_name_offset) {
  if (cu_offset == 0) {
    return false;
  }

  ElfW(Shdr) debug_info;
  constexpr static char kDebugInfoSectionName[] = ".debug_info";
  if (!google::GetSectionHeaderByName(fd, kDebugInfoSectionName,
                                      sizeof(kDebugInfoSectionName),
                                      &debug_info)) {
    return false;
  }
  uint64_t debug_info_end = debug_info.sh_offset + debug_info.sh_size;

  return GetCompileUnitName(fd, debug_info.sh_offset + cu_offset,
                            debug_info_end, pc, base_address, debug_line_offset,
                            cu_name_offset);
}

// Takes the information from `info` and renders the data located in the
// object file `fd` into `out`.  The format looks like:
//
//   [../path/to/foo.cc:10:40]
//
// which would indicate line 10 column 40 in  ../path/to/foo.cc
void SerializeLineNumberInfoToString(int fd,
                                     const LineNumberInfo& info,
                                     char* out,
                                     size_t out_size) {
  size_t out_pos = 0;
  if (info.module_filename_offset) {
    BufferedDwarfReader reader(fd, info.module_dir_offset);
    if (info.module_dir_offset != 0) {
      out_pos +=
          reader.ReadCString(kMaxOffset, out + out_pos, out_size - out_pos);
      out[out_pos - 1] = '/';
    }

    reader.set_position(info.module_filename_offset);
    out_pos +=
        reader.ReadCString(kMaxOffset, out + out_pos, out_size - out_pos);
  } else {
    out[out_pos++] = '\0';
  }

  out[out_pos - 1] = ':';
  auto result = std::to_chars(out + out_pos, out + out_size,
                              static_cast<intptr_t>(info.line));
  if (result.ec != std::errc()) {
    out[out_pos - 1] = '\0';
    return;
  }
  out_pos = static_cast<size_t>(result.ptr - out);

  out[out_pos++] = ':';
  result = std::to_chars(out + out_pos, out + out_size,
                         static_cast<intptr_t>(info.column));
  if (result.ec != std::errc()) {
    out[out_pos - 1] = '\0';
    return;
  }
  out_pos = static_cast<size_t>(result.ptr - out);

  out[out_pos++] = '\0';
}

// Reads the Line Number info for a compile unit.
bool GetLineNumberInfoFromObject(int fd,
                                 uint64_t pc,
                                 uint64_t cu_offset,
                                 uint64_t base_address,
                                 char* out,
                                 size_t out_size) {
  uint64_t cu_name_offset;
  uint64_t debug_line_offset;
  if (!ReadCompileUnit(fd, pc, cu_offset, base_address, &debug_line_offset,
                       &cu_name_offset)) {
    return false;
  }

  ElfW(Shdr) debug_line;
  constexpr static char kDebugLineSectionName[] = ".debug_line";
  if (!google::GetSectionHeaderByName(fd, kDebugLineSectionName,
                                      sizeof(kDebugLineSectionName),
                                      &debug_line)) {
    return false;
  }

  LineNumberInfo info;
  info.pc = pc;
  uint64_t line_info_program_offset = debug_line.sh_offset + debug_line_offset;
  GetLineNumbersInProgram(fd, &info, base_address, line_info_program_offset,
                          cu_name_offset);

  if (info.line == 0) {
    // No matching line number or filename found.
    return false;
  }

  SerializeLineNumberInfoToString(fd, info, out, out_size);

  return true;
}

struct FrameInfo {
  raw_ptr<uint64_t> cu_offset;
  uintptr_t pc;
};

// Returns the number of frames still missing info.
//
// The aranges table is a mapping of ranges to compilation units. Given an array
// of `frame_info`, this finds the compile units for each of the frames doing
// only one pass over the table. It does not preserve the order of `frame_info`.
//
// The main benefit of this function is preserving the single pass through the
// table which is important for performance.
size_t ProcessFlatArangeSet(BufferedDwarfReader* reader,
                            uint64_t next_set,
                            uint8_t address_size,
                            uint64_t base_address,
                            uint64_t cu_offset,
                            FrameInfo* frame_info,
                            size_t num_frames) {
  size_t unsorted_start = 0;
  while (unsorted_start < num_frames && reader->position() < next_set) {
    uint64_t start;
    uint64_t length;
    if (!reader->ReadAddress(address_size, start)) {
      break;
    }
    if (!reader->ReadAddress(address_size, length)) {
      break;
    }
    uint64_t end = start + length;
    for (size_t i = unsorted_start; i < num_frames; ++i) {
      uint64_t module_relative_pc = frame_info[i].pc - base_address;
      if (start <= module_relative_pc && module_relative_pc < end) {
        *frame_info[i].cu_offset = cu_offset;
        if (i != unsorted_start) {
          // Move to sorted section.
          std::swap(frame_info[i], frame_info[unsorted_start]);
        }
        unsorted_start++;
      }
    }
  }

  return unsorted_start;
}

// This is a pre-step that uses the .debug_aranges table to find all the compile
// units for a given set of frames. This allows code to avoid iterating over
// all compile units at a later step in the symbolization process.
void PopulateCompileUnitOffsets(int fd,
                                FrameInfo* frame_info,
                                size_t num_frames,
                                uint64_t base_address) {
  ElfW(Shdr) debug_aranges;
  constexpr static char kDebugArangesSectionName[] = ".debug_aranges";
  if (!google::GetSectionHeaderByName(fd, kDebugArangesSectionName,
                                      sizeof(kDebugArangesSectionName),
                                      &debug_aranges)) {
    return;
  }
  uint64_t debug_aranges_end = debug_aranges.sh_offset + debug_aranges.sh_size;
  uint64_t next_arange_set = kMaxOffset;
  size_t unsorted_start = 0;
  for (BufferedDwarfReader reader(fd, debug_aranges.sh_offset);
       unsorted_start < num_frames && reader.position() < debug_aranges_end;
       reader.set_position(next_arange_set)) {
    bool is_64bit;
    uint64_t length;
    uint16_t arange_version;
    uint64_t debug_info_offset;
    uint8_t address_size;
    if (!reader.ReadCommonHeader(is_64bit, length, arange_version,
                                 debug_info_offset, address_size,
                                 next_arange_set)) {
      return;
    }

    uint8_t segment_size;
    if (!reader.ReadInt8(segment_size)) {
      return;
    }

    if (segment_size != 0) {
      // Only flat namespaces are supported.
      return;
    }

    // The tuple list is aligned, to a multiple of the tuple-size after the
    // section sstart. Because this code only supports flat address spaces, this
    // means 2*address_size.
    while (((reader.position() - debug_aranges.sh_offset) %
            (2 * address_size)) != 0) {
      uint8_t dummy;
      if (!reader.ReadInt8(dummy)) {
        return;
      }
    }
    unsorted_start += ProcessFlatArangeSet(
        &reader, next_arange_set, address_size, base_address, debug_info_offset,
        &frame_info[unsorted_start], num_frames - unsorted_start);
  }
}

}  // namespace

bool GetDwarfSourceLineNumber(const void* pc,
                              uint64_t cu_offset,
                              char* out,
                              size_t out_size) {
  uint64_t pc0 = reinterpret_cast<uint64_t>(pc);
  uint64_t object_start_address = 0;
  uint64_t object_base_address = 0;

  google::FileDescriptor object_fd(google::FileDescriptor(
      google::OpenObjectFileContainingPcAndGetStartAddress(
          pc0, object_start_address, object_base_address, nullptr, 0)));

  if (!object_fd.get()) {
    return false;
  }

  if (!GetLineNumberInfoFromObject(object_fd.get(), pc0, cu_offset,
                                   object_base_address, out, out_size)) {
    return false;
  }

  return true;
}

void GetDwarfCompileUnitOffsets(const void* const* trace,
                                uint64_t* cu_offsets,
                                size_t num_frames) {
  // LINT.IfChange(max_stack_frames)
  FrameInfo frame_info[250] = {};
  // LINT.ThenChange(stack_trace.h:max_stack_frames)
  for (size_t i = 0; i < num_frames; i++) {
    // The `cu_offset` also encodes the original sort order.
    frame_info[i].cu_offset = &cu_offsets[i];
    frame_info[i].pc = reinterpret_cast<uintptr_t>(trace[i]);
  }
  auto pc_comparator = [](const FrameInfo& lhs, const FrameInfo& rhs) {
    return lhs.pc < rhs.pc;
  };

  // Use heapsort to avoid recursion in a signal handler.
  std::make_heap(&frame_info[0], &frame_info[num_frames - 1], pc_comparator);
  std::sort_heap(&frame_info[0], &frame_info[num_frames - 1], pc_comparator);

  // Walk the frame_info one compilation unit at a time.
  for (size_t cur_frame = 0; cur_frame < num_frames; ++cur_frame) {
    uint64_t object_start_address = 0;
    uint64_t object_base_address = 0;
    google::FileDescriptor object_fd(google::FileDescriptor(
        google::OpenObjectFileContainingPcAndGetStartAddress(
            frame_info[cur_frame].pc, object_start_address, object_base_address,
            nullptr, 0)));

    // Some stack frames may not have a corresponding object file, e.g. a call
    // frame inside the Linux kernel's vdso. Just skip over these stack frames,
    // as this is done on a best-effort basis.
    if (object_fd.get() < 0) {
      continue;
    }

    // TODO(crbug.com/40228616): Consider exposing the end address so a
    // range of frames can be bulk-populated. This was originally implemented,
    // but line number symbolization is currently broken by default (and also
    // broken in sandboxed processes). The various issues will be addressed
    // incrementally in follow-up patches, and the optimization here restored if
    // needed.

    PopulateCompileUnitOffsets(object_fd.get(), &frame_info[cur_frame], 1,
                               object_base_address);
  }
}

}  // namespace debug
}  // namespace base

#else  // USE_SYMBOLIZE

#include <cstring>

namespace base {
namespace debug {

bool GetDwarfSourceLineNumber(const void* pc,
                              uint64_t cu_offset,
                              char* out,
                              size_t out_size) {
  return false;
}

void GetDwarfCompileUnitOffsets(const void* const* trace,
                                uint64_t* cu_offsets,
                                size_t num_frames) {
  // Provide defined values even in the stub.
  memset(cu_offsets, 0, sizeof(cu_offsets) * num_frames);
}

}  // namespace debug
}  // namespace base

#endif
