// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/module_cache.h"

#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <string.h>
#include <uuid/uuid.h>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

namespace base {

namespace {

#if defined(ARCH_CPU_64_BITS)
using MachHeaderType = mach_header_64;
using SegmentCommandType = segment_command_64;
constexpr uint32_t kMachHeaderMagic = MH_MAGIC_64;
constexpr uint32_t kSegmentCommand = LC_SEGMENT_64;
#else
using MachHeaderType = mach_header;
using SegmentCommandType = segment_command;
constexpr uint32_t kMachHeaderMagic = MH_MAGIC;
constexpr uint32_t kSegmentCommand = LC_SEGMENT;
#endif

// Returns the unique build ID and text segment size for a module loaded at
// |module_addr|. Returns the empty string and 0 if the function fails to get
// the build ID or size.
//
// Build IDs are created by the concatenation of the module's GUID (Windows) /
// UUID (Mac) and an "age" field that indicates how many times that GUID/UUID
// has been reused. In Windows binaries, the "age" field is present in the
// module header, but on the Mac, UUIDs are never reused and so the "age" value
// appended to the UUID is always 0.
void GetUniqueIdAndTextSize(const void* module_addr,
                            std::string* unique_id,
                            size_t* text_size) {
  const MachHeaderType* mach_header =
      reinterpret_cast<const MachHeaderType*>(module_addr);
  DCHECK_EQ(mach_header->magic, kMachHeaderMagic);

  size_t offset = sizeof(MachHeaderType);
  size_t offset_limit = sizeof(MachHeaderType) + mach_header->sizeofcmds;
  bool found_uuid = false;
  bool found_text_size = false;

  for (uint32_t i = 0; i < mach_header->ncmds; ++i) {
    if (offset + sizeof(load_command) >= offset_limit) {
      unique_id->clear();
      *text_size = 0;
      return;
    }

    const load_command* load_cmd = reinterpret_cast<const load_command*>(
        reinterpret_cast<const uint8_t*>(mach_header) + offset);

    if (offset + load_cmd->cmdsize > offset_limit) {
      // This command runs off the end of the command list. This is malformed.
      unique_id->clear();
      *text_size = 0;
      return;
    }

    if (load_cmd->cmd == LC_UUID) {
      if (load_cmd->cmdsize < sizeof(uuid_command)) {
        // This "UUID command" is too small. This is malformed.
        unique_id->clear();
      } else {
        const uuid_command* uuid_cmd =
            reinterpret_cast<const uuid_command*>(load_cmd);
        static_assert(sizeof(uuid_cmd->uuid) == sizeof(uuid_t),
                      "UUID field of UUID command should be 16 bytes.");
        // The ID comprises the UUID concatenated with the Mac's "age" value
        // which is always 0.
        unique_id->assign(HexEncode(&uuid_cmd->uuid, sizeof(uuid_cmd->uuid)) +
                          "0");
      }
      if (found_text_size) {
        return;
      }
      found_uuid = true;
    } else if (load_cmd->cmd == kSegmentCommand) {
      const SegmentCommandType* segment_cmd =
          reinterpret_cast<const SegmentCommandType*>(load_cmd);
      if (strncmp(segment_cmd->segname, SEG_TEXT,
                  sizeof(segment_cmd->segname)) == 0) {
        *text_size = segment_cmd->vmsize;
        // Compare result with library function call, which is slower than this
        // code.
        unsigned long text_size_from_libmacho;
        DCHECK(getsegmentdata(mach_header, SEG_TEXT, &text_size_from_libmacho));
        DCHECK_EQ(*text_size, text_size_from_libmacho);
      }
      if (found_uuid) {
        return;
      }
      found_text_size = true;
    }
    offset += load_cmd->cmdsize;
  }

  if (!found_uuid) {
    unique_id->clear();
  }
  if (!found_text_size) {
    *text_size = 0;
  }
}

}  // namespace

class MacModule : public ModuleCache::Module {
 public:
  explicit MacModule(const Dl_info& dl_info)
      : base_address_(reinterpret_cast<uintptr_t>(dl_info.dli_fbase)),
        debug_basename_(FilePath(dl_info.dli_fname).BaseName()) {
    GetUniqueIdAndTextSize(dl_info.dli_fbase, &id_, &size_);
  }

  MacModule(const MacModule&) = delete;
  MacModule& operator=(const MacModule&) = delete;

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return id_; }
  FilePath GetDebugBasename() const override { return debug_basename_; }
  size_t GetSize() const override { return size_; }
  bool IsNative() const override { return true; }

 private:
  uintptr_t base_address_;
  std::string id_;
  FilePath debug_basename_;
  size_t size_;
};

// static
std::unique_ptr<const ModuleCache::Module> ModuleCache::CreateModuleForAddress(
    uintptr_t address) {
  Dl_info info;
  if (!dladdr(reinterpret_cast<const void*>(address), &info)) {
    return nullptr;
  }
  return std::make_unique<MacModule>(info);
}

}  // namespace base
