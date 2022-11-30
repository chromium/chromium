// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/pe_image.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// The maximum amount of bytes that can be reported as modified by VerifyModule.
const int kMaxModuleModificationBytes = 256;

struct Export {
  Export(void* addr, const std::string& name);
  ~Export();

  bool operator<(const Export& other) const {
    return addr < other.addr;
  }

  raw_ptr<void> addr;
  std::string name;
};

Export::Export(void* addr, const std::string& name) : addr(addr), name(name) {
}

Export::~Export() {
}

struct ModuleVerificationState {
  explicit ModuleVerificationState(HMODULE hModule);

  ModuleVerificationState(const ModuleVerificationState&) = delete;
  ModuleVerificationState& operator=(const ModuleVerificationState&) = delete;

  ~ModuleVerificationState();

  base::win::PEImageAsData disk_peimage;

  // The module's preferred base address minus the base address it actually
  // loaded at.
  intptr_t image_base_delta;

  // The location of the disk_peimage module's code section minus that of the
  // mem_peimage module's code section.
  intptr_t code_section_delta;

  // Set true if the relocation table contains a reloc of type that we don't
  // currently handle.
  bool unknown_reloc_type;

  // The start of the code section of the in-memory binary.
  uint8_t* mem_code_addr;

  // The start of the code section of the on-disk binary.
  uint8_t* disk_code_addr;

  // The size of the binary's code section.
  uint32_t code_size;

  // The exports of the DLL, sorted by address in ascending order.
  std::vector<Export> exports;

  // The location in the in-memory binary of the latest reloc encountered by
  // |EnumRelocsCallback|.
  raw_ptr<uint8_t> last_mem_reloc_position;

  // The location in the on-disk binary of the latest reloc encountered by
  // |EnumRelocsCallback|.
  raw_ptr<uint8_t> last_disk_reloc_position;

  // The number of bytes with a different value on disk and in memory, as
  // computed by |VerifyModule|.
  int bytes_different;

  // The module state protobuf object that |VerifyModule| will populate.
  raw_ptr<ClientIncidentReport_EnvironmentData_Process_ModuleState>
      module_state;
};

ModuleVerificationState::ModuleVerificationState(HMODULE hModule)
    : disk_peimage(hModule),
      image_base_delta(0),
      code_section_delta(0),
      unknown_reloc_type(false),
      mem_code_addr(nullptr),
      disk_code_addr(nullptr),
      code_size(0),
      last_mem_reloc_position(nullptr),
      last_disk_reloc_position(nullptr),
      bytes_different(0),
      module_state(nullptr) {
}

ModuleVerificationState::~ModuleVerificationState() {
}

// Find which export a modification at address |mem_address| is in. Looks for
// the largest export address still smaller than |mem_address|. |start| and
// |end| must come from a sorted collection.
std::vector<Export>::const_iterator FindModifiedExport(
    uint8_t* mem_address,
    std::vector<Export>::const_iterator start,
    std::vector<Export>::const_iterator end) {
  // We get the largest export address still smaller than |addr|.  It is
  // possible that |addr| belongs to some nonexported function located
  // between this export and the following one.
  Export addr(reinterpret_cast<void*>(mem_address), std::string());
  return std::upper_bound(start, end, addr);
}

// Checks each byte in a subsection of the module's code section against the
// corresponding byte on disk, returning the number of bytes differing between
// the two. |state.exports| must be sorted.
int ExamineByteRangeDiff(uint8_t* disk_start,
                         uint8_t* mem_start,
                         ptrdiff_t range_size,
                         ModuleVerificationState* state) {
  int bytes_different = 0;
  std::vector<Export>::const_iterator export_it = state->exports.begin();

  for (uint8_t* end = mem_start + range_size; mem_start < end;
       ++mem_start, ++disk_start) {
    if (*disk_start == *mem_start)
      continue;

    auto* modification = state->module_state->add_modification();
    // Store the address at which the modification starts on disk, relative to
    // the beginning of the image.
    modification->set_file_offset(
        disk_start - reinterpret_cast<uint8_t*>(state->disk_peimage.module()));

    // Find the export containing this modification.
    std::vector<Export>::const_iterator modified_export_it =
        FindModifiedExport(mem_start, export_it, state->exports.end());
    // No later byte can belong to an earlier export.
    export_it = modified_export_it;
    if (modified_export_it != state->exports.begin())
      modification->set_export_name((modified_export_it - 1)->name);

    const uint8_t* range_start = mem_start;
    while (mem_start < end && *disk_start != *mem_start) {
      ++disk_start;
      ++mem_start;
    }
    int bytes_in_modification = mem_start - range_start;
    bytes_different += bytes_in_modification;
    modification->set_byte_count(bytes_in_modification);
    modification->set_modified_bytes(
        range_start,
        std::min(bytes_in_modification, kMaxModuleModificationBytes));
  }
  return bytes_different;
}

bool AddrIsInCodeSection(void* address,
                         uint8_t* code_addr,
                         uint32_t code_size) {
  return (code_addr <= address && address < code_addr + code_size);
}

bool EnumRelocsCallback(const base::win::PEImage& mem_peimage,
                        WORD type,
                        void* address,
                        void* cookie) {
  ModuleVerificationState* state =
      reinterpret_cast<ModuleVerificationState*>(cookie);

  // If not in the code section return true to continue to the next reloc.
  if (!AddrIsInCodeSection(address, state->mem_code_addr, state->code_size))
    return true;

  switch (type) {
    case IMAGE_REL_BASED_ABSOLUTE:  // 0
      break;
    case IMAGE_REL_BASED_HIGHLOW:  // 3
      {
        // The range to inspect is from the last reloc to the current one at
        // |ptr|
        uint8_t* ptr = reinterpret_cast<uint8_t*>(address);

        // If the last relocation was not before this one in the binary,
        // there's an issue in the reloc section. We can't really recover from
        // that so flag state as such so the error can be logged.
        if (ptr < state->last_mem_reloc_position)
          return false;

        // Check which bytes of the relocation are not accounted for by the
        // rebase. If the beginning of the relocation is modified by something
        // other than the rebase, extend the verification range to include those
        // bytes since they are considered part of a modification.
        uint32_t relocated = *reinterpret_cast<uint32_t*>(ptr);
        uint32_t original = relocated + state->image_base_delta;
        uint8_t* original_reloc_bytes = reinterpret_cast<uint8_t*>(&original);
        uint8_t* reloc_disk_position = ptr + state->code_section_delta;
        size_t unaccounted_reloc_bytes = 0;
        while (unaccounted_reloc_bytes < sizeof(uint32_t) &&
               original_reloc_bytes[unaccounted_reloc_bytes] !=
               reloc_disk_position[unaccounted_reloc_bytes]) {
          ++unaccounted_reloc_bytes;
        }

        // If the entire reloc was modified, return true to let the next
        // EnumReloc track it as part of a larger modification.
        if (unaccounted_reloc_bytes == sizeof(uint32_t))
          return true;

        ptrdiff_t range_size = ptr +
                               unaccounted_reloc_bytes -
                               state->last_mem_reloc_position;

        state->bytes_different += ExamineByteRangeDiff(
            state->last_disk_reloc_position,
            state->last_mem_reloc_position,
            range_size,
            state);

        // Starting after the verified range, check if the relocation ends with
        // modified bytes. If it does, include them in the following range to be
        // verified as they're considered modified. Otherwise, the following
        // range will start right after the current reloc.
        size_t unmodified_reloc_byte_count = unaccounted_reloc_bytes;
        while (unmodified_reloc_byte_count < sizeof(uint32_t) &&
               original_reloc_bytes[unmodified_reloc_byte_count] ==
               reloc_disk_position[unmodified_reloc_byte_count]) {
          ++unmodified_reloc_byte_count;
        }
        state->last_disk_reloc_position +=
            range_size + unmodified_reloc_byte_count;
        state->last_mem_reloc_position +=
            range_size + unmodified_reloc_byte_count;
      }
      break;
    case IMAGE_REL_BASED_DIR64:  // 10
      break;
    default:
      // TODO(robertshield): Find a reliable description of the behaviour of the
      // remaining types of relocation and handle them.
      state->unknown_reloc_type = true;
      break;
  }
  return true;
}

bool EnumExportsCallback(const base::win::PEImage& mem_peimage,
                         DWORD ordinal,
                         DWORD hint,
                         LPCSTR name,
                         PVOID function_addr,
                         LPCSTR forward,
                         PVOID cookie) {
  std::vector<Export>* exports = reinterpret_cast<std::vector<Export>*>(cookie);
  if (name)
    exports->push_back(Export(function_addr, std::string(name)));
  return true;
}

}  // namespace

bool GetCodeAddrsAndSize(const base::win::PEImage& mem_peimage,
                         const base::win::PEImageAsData& disk_peimage,
                         uint8_t** mem_code_addr,
                         uint8_t** disk_code_addr,
                         uint32_t* code_size) {
  DWORD base_of_code = mem_peimage.GetNTHeaders()->OptionalHeader.BaseOfCode;

  // Get the address and size of the code section in the loaded module image.
  PIMAGE_SECTION_HEADER mem_code_header =
      mem_peimage.GetImageSectionFromAddr(mem_peimage.RVAToAddr(base_of_code));
  if (mem_code_header == NULL)
    return false;
  *mem_code_addr = reinterpret_cast<uint8_t*>(
      mem_peimage.RVAToAddr(mem_code_header->VirtualAddress));
  // If the section is padded with zeros when mapped then |VirtualSize| can be
  // larger.  Alternatively, |SizeOfRawData| can be rounded up to align
  // according to OptionalHeader.FileAlignment.
  *code_size = std::min(mem_code_header->Misc.VirtualSize,
                        mem_code_header->SizeOfRawData);

  // Get the address of the code section in the module mapped as data from disk.
  DWORD disk_code_offset = 0;
  if (!mem_peimage.ImageAddrToOnDiskOffset(
          reinterpret_cast<void*>(*mem_code_addr), &disk_code_offset))
    return false;
  *disk_code_addr =
      reinterpret_cast<uint8_t*>(disk_peimage.module()) + disk_code_offset;
  return true;
}

bool VerifyModule(
    const wchar_t* module_name,
    ClientIncidentReport_EnvironmentData_Process_ModuleState* module_state,
    int* num_bytes_different) {
  using ModuleState = ClientIncidentReport_EnvironmentData_Process_ModuleState;
  *num_bytes_different = 0;
  module_state->set_name(base::WideToUTF8(module_name));
  module_state->set_modified_state(ModuleState::MODULE_STATE_UNKNOWN);

  // Get module handle, load a copy from disk as data and create PEImages.
  HMODULE module_handle = NULL;
  if (!GetModuleHandleEx(0, module_name, &module_handle))
    return false;
  base::ScopedNativeLibrary native_library(module_handle);

  WCHAR module_path[MAX_PATH] = {};
  DWORD length =
      GetModuleFileName(module_handle, module_path, std::size(module_path));
  if (!length || length == std::size(module_path))
    return false;

  base::MemoryMappedFile mapped_module;
  if (!mapped_module.Initialize(base::FilePath(module_path)))
    return false;
  ModuleVerificationState state(
      reinterpret_cast<HMODULE>(const_cast<uint8_t*>(mapped_module.data())));

  base::win::PEImage mem_peimage(module_handle);
  if (!mem_peimage.VerifyMagic() || !state.disk_peimage.VerifyMagic())
    return false;

  // Get the list of exports and sort them by address for efficient lookups.
  mem_peimage.EnumExports(EnumExportsCallback, &state.exports);
  std::sort(state.exports.begin(), state.exports.end());

  // Get the addresses of the code sections then calculate |code_section_delta|
  // and |image_base_delta|.
  if (!GetCodeAddrsAndSize(mem_peimage,
                           state.disk_peimage,
                           &state.mem_code_addr,
                           &state.disk_code_addr,
                           &state.code_size))
    return false;

  state.module_state = module_state;
  state.last_mem_reloc_position = state.mem_code_addr;
  state.last_disk_reloc_position = state.disk_code_addr;
  state.code_section_delta = state.disk_code_addr - state.mem_code_addr;

  uint8_t* preferred_image_base = reinterpret_cast<uint8_t*>(
      state.disk_peimage.GetNTHeaders()->OptionalHeader.ImageBase);
  state.image_base_delta =
      preferred_image_base - reinterpret_cast<uint8_t*>(mem_peimage.module());

  state.last_mem_reloc_position = state.mem_code_addr;
  state.last_disk_reloc_position = state.disk_code_addr;

  // Enumerate relocations and verify the bytes between them.
  bool scan_complete = mem_peimage.EnumRelocs(EnumRelocsCallback, &state);

  if (scan_complete) {
    size_t range_size =
        state.code_size - (state.last_mem_reloc_position - state.mem_code_addr);
    // Inspect the last chunk spanning from the furthest relocation to the end
    // of the code section.
    state.bytes_different += ExamineByteRangeDiff(
        state.last_disk_reloc_position,
        state.last_mem_reloc_position,
        range_size,
        &state);
  }
  *num_bytes_different = state.bytes_different;

  // Report STATE_MODIFIED if any difference was found, regardless of whether or
  // not the entire module was scanned. Report STATE_UNMODIFIED only if the
  // entire module was scanned and understood.
  if (state.bytes_different)
    module_state->set_modified_state(ModuleState::MODULE_STATE_MODIFIED);
  else if (!state.unknown_reloc_type && scan_complete)
    module_state->set_modified_state(ModuleState::MODULE_STATE_UNMODIFIED);

  return scan_complete;
}

}  // namespace safe_browsing
