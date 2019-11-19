// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/pe_image.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

// A scoper that makes a modification at a given address when constructed, and
// reverts it upon destruction.
template <size_t ModificationLength>
class ScopedModuleModifier {
 public:
  explicit ScopedModuleModifier(uint8_t* address) : address_(address) {
    uint8_t modification[ModificationLength];
    std::transform(address, address + ModificationLength, &modification[0],
                   [](uint8_t byte) { return byte + 1U; });
    SIZE_T bytes_written = 0;
    EXPECT_NE(0, WriteProcessMemory(GetCurrentProcess(),
                                    address,
                                    &modification[0],
                                    ModificationLength,
                                    &bytes_written));
    EXPECT_EQ(ModificationLength, bytes_written);
  }

  ~ScopedModuleModifier() {
    uint8_t modification[ModificationLength];
    std::transform(address_, address_ + ModificationLength, &modification[0],
                   [](uint8_t byte) { return byte - 1U; });
    SIZE_T bytes_written = 0;
    EXPECT_NE(0, WriteProcessMemory(GetCurrentProcess(),
                                    address_,
                                    &modification[0],
                                    ModificationLength,
                                    &bytes_written));
    EXPECT_EQ(ModificationLength, bytes_written);
  }

 private:
  uint8_t* address_;

  DISALLOW_COPY_AND_ASSIGN(ScopedModuleModifier);
};

}  // namespace

class SafeBrowsingModuleVerifierWinTest : public testing::Test {
 protected:
  using ModuleState = ClientIncidentReport_EnvironmentData_Process_ModuleState;

  // A mapping of an export name to the sequence of modifications for it.
  using ExportNameToModifications =
      std::map<std::string, std::vector<const ModuleState::Modification*>>;

  void SetUpTestDllAndPEImages() {
    LoadModule();
    HMODULE mem_handle;
    GetMemModuleHandle(&mem_handle);
    mem_peimage_ptr_.reset(new base::win::PEImage(mem_handle));
    ASSERT_TRUE(mem_peimage_ptr_->VerifyMagic());

    LoadDLLAsFile();
    HMODULE disk_handle;
    GetDiskModuleHandle(&disk_handle);
    disk_peimage_ptr_.reset(new base::win::PEImageAsData(disk_handle));
    ASSERT_TRUE(disk_peimage_ptr_->VerifyMagic());
  }

  void LoadModule() {
    HMODULE mem_dll_handle =
        LoadNativeLibrary(base::FilePath(kTestDllNames[0]), NULL);
    ASSERT_NE(static_cast<HMODULE>(NULL), mem_dll_handle)
        << "GLE=" << GetLastError();
    mem_dll_handle_ = base::ScopedNativeLibrary(mem_dll_handle);
    ASSERT_TRUE(mem_dll_handle_.is_valid());
  }

  void GetMemModuleHandle(HMODULE* mem_handle) {
    *mem_handle = GetModuleHandle(kTestDllNames[0]);
    ASSERT_NE(static_cast<HMODULE>(NULL), *mem_handle);
  }

  void LoadDLLAsFile() {
    // Use the module handle to find the it on disk, then load as a file.
    HMODULE module_handle;
    GetMemModuleHandle(&module_handle);

    WCHAR module_path[MAX_PATH] = {};
    DWORD length =
        GetModuleFileName(module_handle, module_path, base::size(module_path));
    ASSERT_NE(base::size(module_path), length);
    ASSERT_TRUE(disk_dll_handle_.Initialize(base::FilePath(module_path)));
  }

  void GetDiskModuleHandle(HMODULE* disk_handle) {
    *disk_handle = reinterpret_cast<HMODULE>(
        const_cast<uint8_t*>(disk_dll_handle_.data()));
  }

  // Returns the address of the named function exported by the test dll.
  uint8_t* GetAddressOfExport(const char* export_name) {
    HMODULE mem_handle;
    GetMemModuleHandle(&mem_handle);
    uint8_t* export_addr =
        reinterpret_cast<uint8_t*>(GetProcAddress(mem_handle, export_name));
    EXPECT_NE(nullptr, export_addr);
    return export_addr;
  }

  static void AssertModuleUnmodified(const ModuleState& state,
                                     const wchar_t* module_name) {
    ASSERT_TRUE(state.has_name());
    ASSERT_EQ(base::WideToUTF8(module_name), state.name());
    ASSERT_TRUE(state.has_modified_state());
    ASSERT_EQ(ModuleState::MODULE_STATE_UNMODIFIED, state.modified_state());
    ASSERT_EQ(0, state.modification_size());
  }

  // Replaces the contents of |modification_map| with pointers to those in
  // |state|. |state| must outlive |modification_map|.
  static void BuildModificationMap(
      const ModuleState& state,
      ExportNameToModifications* modification_map) {
    modification_map->clear();
    std::string export_name;
    for (auto& modification : state.modification()) {
      if (!modification.has_export_name())
        export_name.clear();
      else
        export_name = modification.export_name();
      (*modification_map)[export_name].push_back(&modification);
    }
  }

  base::ScopedNativeLibrary mem_dll_handle_;
  base::MemoryMappedFile disk_dll_handle_;
  std::unique_ptr<base::win::PEImageAsData> disk_peimage_ptr_;
  std::unique_ptr<base::win::PEImage> mem_peimage_ptr_;
};

// Don't run these tests under AddressSanitizer as it patches the modules on
// startup, thus interferes with all these test expectations.
#if !defined(ADDRESS_SANITIZER)
TEST_F(SafeBrowsingModuleVerifierWinTest, VerifyModuleUnmodified) {
  ModuleState state;
  int num_bytes_different = 0;
  // Call VerifyModule before the module has been loaded, should fail.
  ASSERT_FALSE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_name());
  ASSERT_EQ(base::WideToUTF8(kTestDllNames[0]), state.name());
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_UNKNOWN, state.modified_state());
  ASSERT_EQ(0, num_bytes_different);
  ASSERT_EQ(0, state.modification_size());

  // On loading, the module should be identical (up to relocations) in memory as
  // on disk.
  SetUpTestDllAndPEImages();
  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  AssertModuleUnmodified(state, kTestDllNames[0]);
  ASSERT_EQ(0, num_bytes_different);
}

// Flaky in debug builds; see https://crbug.com/877815.
#if !defined(NDEBUG)
#define MAYBE_VerifyModuleModified DISABLED_VerifyModuleModified
#else
#define MAYBE_VerifyModuleModified VerifyModuleModified
#endif
TEST_F(SafeBrowsingModuleVerifierWinTest, MAYBE_VerifyModuleModified) {
  int num_bytes_different = 0;
  ModuleState state;

  SetUpTestDllAndPEImages();
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  AssertModuleUnmodified(state, kTestDllNames[0]);
  ASSERT_EQ(0, num_bytes_different);

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));

  ScopedModuleModifier<1> mod(mem_code_addr);

  size_t modification_offset = code_size - 1;
  ScopedModuleModifier<1> mod2(mem_code_addr + modification_offset);

  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_name());
  ASSERT_EQ(base::WideToUTF8(kTestDllNames[0]), state.name());
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(2, num_bytes_different);
  ASSERT_EQ(2, state.modification_size());

  size_t expected_file_offset =
      disk_code_addr - reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());
  EXPECT_EQ(1, state.modification(0).byte_count());
  EXPECT_EQ(mem_code_addr[0],
            (uint8_t)state.modification(0).modified_bytes()[0]);

  expected_file_offset = (disk_code_addr + modification_offset) -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(1).file_offset());
  EXPECT_EQ(1, state.modification(1).byte_count());
  EXPECT_EQ(mem_code_addr[modification_offset],
            (uint8_t)state.modification(1).modified_bytes()[0]);
}

// TODO(crbug.com/838124) The test is flaky on Win7 debug.
#if !defined(NDEBUG)
#define MAYBE_VerifyModuleLongModification DISABLED_VerifyModuleLongModification
#else
#define MAYBE_VerifyModuleLongModification VerifyModuleLongModification
#endif

TEST_F(SafeBrowsingModuleVerifierWinTest, MAYBE_VerifyModuleLongModification) {
  ModuleState state;
  int num_bytes_different = 0;

  SetUpTestDllAndPEImages();
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  AssertModuleUnmodified(state, kTestDllNames[0]);
  ASSERT_EQ(0, num_bytes_different);

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));

  const int kModificationSize = 256;
  // Write the modification at the end so it's not overlapping relocations
  const size_t modification_offset = code_size - kModificationSize;
  ScopedModuleModifier<kModificationSize> mod(
      mem_code_addr + modification_offset);

  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_name());
  ASSERT_EQ(base::WideToUTF8(kTestDllNames[0]), state.name());
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(kModificationSize, num_bytes_different);
  ASSERT_EQ(1, state.modification_size());

  EXPECT_EQ(kModificationSize, state.modification(0).byte_count());

  size_t expected_file_offset = disk_code_addr + modification_offset -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());

  EXPECT_EQ(
      std::string(mem_code_addr + modification_offset,
                  mem_code_addr + modification_offset + kModificationSize),
      state.modification(0).modified_bytes());
}

TEST_F(SafeBrowsingModuleVerifierWinTest, VerifyModuleRelocOverlap) {
  int num_bytes_different = 0;
  ModuleState state;

  SetUpTestDllAndPEImages();
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  AssertModuleUnmodified(state, kTestDllNames[0]);
  ASSERT_EQ(0, num_bytes_different);

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));

  // Modify the first hunk of the code, which contains many relocs.
  const int kModificationSize = 256;
  ScopedModuleModifier<kModificationSize> mod(mem_code_addr);

  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_name());
  ASSERT_EQ(base::WideToUTF8(kTestDllNames[0]), state.name());
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(kModificationSize, num_bytes_different);

  // Modifications across the relocs should have been coalesced into one.
  ASSERT_EQ(1, state.modification_size());
  ASSERT_EQ(kModificationSize, state.modification(0).byte_count());
  ASSERT_EQ(static_cast<size_t>(kModificationSize),
            state.modification(0).modified_bytes().size());
  EXPECT_EQ(std::string(mem_code_addr, mem_code_addr + kModificationSize),
            state.modification(0).modified_bytes());
}

// Flaky in debug builds; see https://crbug.com/877815.
#if !defined(NDEBUG)
#define MAYBE_VerifyModuleExportModified DISABLED_VerifyModuleExportModified
#else
#define MAYBE_VerifyModuleExportModified VerifyModuleExportModified
#endif
TEST_F(SafeBrowsingModuleVerifierWinTest, MAYBE_VerifyModuleExportModified) {
  ModuleState state;
  int num_bytes_different = 0;
  // Confirm the module is identical in memory as on disk before we begin.
  SetUpTestDllAndPEImages();
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  AssertModuleUnmodified(state, kTestDllNames[0]);
  ASSERT_EQ(0, num_bytes_different);

  // Edit one exported function. VerifyModule should now return the function
  // name in the modification.
  ScopedModuleModifier<1> mod(GetAddressOfExport(kTestExportName));
  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_name());
  ASSERT_EQ(base::WideToUTF8(kTestDllNames[0]), state.name());
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(1, state.modification_size());

  // Extract the offset of this modification.
  ExportNameToModifications modification_map;
  BuildModificationMap(state, &modification_map);
  ASSERT_EQ(1U, modification_map[kTestExportName].size());
  uint32_t export_offset = modification_map[kTestExportName][0]->file_offset();

  // Edit another exported function. VerifyModule should now report both. Add
  // one to the address so that this modification and the previous are not
  // coalesced in the event that the first export is only one byte (e.g., ret).
  ScopedModuleModifier<1> mod2(GetAddressOfExport(kTestDllMainExportName) + 1);
  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(2, state.modification_size());

  // The first modification should be present and unmodified.
  BuildModificationMap(state, &modification_map);
  ASSERT_EQ(1U, modification_map[kTestExportName].size());
  ASSERT_EQ(export_offset, modification_map[kTestExportName][0]->file_offset());

  // The second modification should be present and different than the first.
  ASSERT_EQ(1U, modification_map[kTestDllMainExportName].size());
  ASSERT_NE(export_offset,
            modification_map[kTestDllMainExportName][0]->file_offset());

  // Now make another edit at the very end of the code section. This should be
  // attributed to the last export.
  uint8_t* mem_code_addr = nullptr;
  uint8_t* disk_code_addr = nullptr;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));
  ScopedModuleModifier<1> mod3(mem_code_addr + code_size - 1);

  state.Clear();
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(3, state.modification_size());

  // One of the two exports now has two modifications.
  BuildModificationMap(state, &modification_map);
  ASSERT_EQ(2U, modification_map.size());
  ASSERT_EQ(3U, (modification_map.begin()->second.size() +
                 (++modification_map.begin())->second.size()));
}
#endif  // ADDRESS_SANITIZER

}  // namespace safe_browsing
