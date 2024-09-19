// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"

#include <windows.h>

#include <psapi.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/native_library.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/pe_image.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

// A scoper that makes a modification at a given address when constructed, and
// reverts it upon destruction.
template <size_t ModificationLength>
class ScopedModuleModifier {
 public:
  explicit ScopedModuleModifier(
      base::span<const uint8_t, ModificationLength> address)
      : modification_region_(address) {
    uint8_t modification[ModificationLength];

    base::ranges::transform(modification_region_, std::begin(modification),
                            [](uint8_t byte) { return byte + 1U; });
    SIZE_T bytes_written = 0;
    EXPECT_NE(
        0, WriteProcessMemory(GetCurrentProcess(),
                              const_cast<uint8_t*>(modification_region_.data()),
                              std::begin(modification), ModificationLength,
                              &bytes_written));
    EXPECT_EQ(ModificationLength, bytes_written);
  }

  ScopedModuleModifier(const ScopedModuleModifier&) = delete;
  ScopedModuleModifier& operator=(const ScopedModuleModifier&) = delete;

  ~ScopedModuleModifier() {
    uint8_t modification[ModificationLength];

    base::ranges::transform(modification_region_, std::begin(modification),
                            [](uint8_t byte) { return byte - 1U; });
    SIZE_T bytes_written = 0;
    EXPECT_NE(
        0, WriteProcessMemory(GetCurrentProcess(),
                              const_cast<uint8_t*>(modification_region_.data()),
                              std::begin(modification), ModificationLength,
                              &bytes_written));
    EXPECT_EQ(ModificationLength, bytes_written);
  }

 private:
  base::span<const uint8_t> modification_region_;
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
    mem_peimage_ptr_ = std::make_unique<base::win::PEImage>(mem_handle);
    ASSERT_TRUE(mem_peimage_ptr_->VerifyMagic());

    LoadDLLAsFile();
    HMODULE disk_handle;
    GetDiskModuleHandle(&disk_handle);
    disk_peimage_ptr_ = std::make_unique<base::win::PEImageAsData>(disk_handle);
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
        GetModuleFileName(module_handle, module_path, std::size(module_path));
    ASSERT_NE(std::size(module_path), length);
    ASSERT_TRUE(disk_dll_handle_.Initialize(base::FilePath(module_path)));
  }

  void GetDiskModuleHandle(HMODULE* disk_handle) {
    *disk_handle = reinterpret_cast<HMODULE>(
        const_cast<uint8_t*>(disk_dll_handle_.data()));
  }

  // Returns the data in the module starting with the named function
  // exported by the test dll.
  base::span<uint8_t> GetCodeAfterExport(const char* export_name) {
    HMODULE mem_handle;
    GetMemModuleHandle(&mem_handle);
    MODULEINFO module_info;
    EXPECT_TRUE(::GetModuleInformation(::GetCurrentProcess(), mem_handle,
                                       &module_info, sizeof(module_info)));
    // SAFETY: The module address and size were provided by the OS.
    UNSAFE_BUFFERS(base::span<uint8_t> module_data(
        reinterpret_cast<uint8_t*>(mem_handle), module_info.SizeOfImage));

    uint8_t* export_addr =
        reinterpret_cast<uint8_t*>(GetProcAddress(mem_handle, export_name));
    EXPECT_NE(nullptr, export_addr);

    return module_data.subspan(export_addr -
                               base::to_address(module_data.begin()));
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

  base::span<const uint8_t> mem_code_data;
  base::span<const uint8_t> disk_code_data;
  ASSERT_TRUE(GetCodeSpans(*mem_peimage_ptr_, disk_dll_handle_.bytes(),
                           mem_code_data, disk_code_data));

  ScopedModuleModifier<1> mod(mem_code_data.first<1>());

  size_t modification_offset = mem_code_data.size() - 1;
  ScopedModuleModifier<1> mod2(
      *mem_code_data.subspan(modification_offset, 1).to_fixed_extent<1>());

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
      base::to_address(disk_code_data.begin()) -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());
  EXPECT_EQ(1, state.modification(0).byte_count());
  EXPECT_EQ(mem_code_data[0],
            (uint8_t)state.modification(0).modified_bytes()[0]);

  expected_file_offset =
      base::to_address(disk_code_data.begin()) -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module()) +
      modification_offset;
  EXPECT_EQ(expected_file_offset, state.modification(1).file_offset());
  EXPECT_EQ(1, state.modification(1).byte_count());
  EXPECT_EQ(mem_code_data[modification_offset],
            (uint8_t)state.modification(1).modified_bytes()[0]);
}

TEST_F(SafeBrowsingModuleVerifierWinTest, VerifyModuleLongModification) {
  ModuleState state;
  int num_bytes_different = 0;

  SetUpTestDllAndPEImages();
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  AssertModuleUnmodified(state, kTestDllNames[0]);
  ASSERT_EQ(0, num_bytes_different);

  base::span<const uint8_t> mem_code_data;
  base::span<const uint8_t> disk_code_data;
  ASSERT_TRUE(GetCodeSpans(*mem_peimage_ptr_, disk_dll_handle_.bytes(),
                           mem_code_data, disk_code_data));

  constexpr size_t kModificationSize = 256;
  // Write the modification at the end so it's not overlapping relocations
  const size_t modification_offset = mem_code_data.size() - kModificationSize;
  ScopedModuleModifier<kModificationSize> mod(
      *mem_code_data.subspan(modification_offset, kModificationSize)
           .to_fixed_extent<kModificationSize>());

  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_name());
  ASSERT_EQ(base::WideToUTF8(kTestDllNames[0]), state.name());
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(static_cast<int>(kModificationSize), num_bytes_different);
  ASSERT_EQ(1, state.modification_size());

  EXPECT_EQ(static_cast<int>(kModificationSize),
            state.modification(0).byte_count());

  size_t expected_file_offset =
      base::to_address(disk_code_data.begin()) -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module()) +
      modification_offset;
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());

  EXPECT_EQ(mem_code_data.subspan(modification_offset, kModificationSize),
            base::as_byte_span(state.modification(0).modified_bytes()));
}

TEST_F(SafeBrowsingModuleVerifierWinTest, VerifyModuleRelocOverlap) {
  int num_bytes_different = 0;
  ModuleState state;

  SetUpTestDllAndPEImages();
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  AssertModuleUnmodified(state, kTestDllNames[0]);
  ASSERT_EQ(0, num_bytes_different);

  base::span<const uint8_t> mem_code_data;
  base::span<const uint8_t> disk_code_data;
  ASSERT_TRUE(GetCodeSpans(*mem_peimage_ptr_, disk_dll_handle_.bytes(),
                           mem_code_data, disk_code_data));

  // Modify the first hunk of the code, which contains many relocs.
  constexpr size_t kModificationSize = 256;
  ScopedModuleModifier<kModificationSize> mod(
      mem_code_data.first<kModificationSize>());

  state.Clear();
  num_bytes_different = 0;
  ASSERT_TRUE(VerifyModule(kTestDllNames[0], &state, &num_bytes_different));
  ASSERT_TRUE(state.has_name());
  ASSERT_EQ(base::WideToUTF8(kTestDllNames[0]), state.name());
  ASSERT_TRUE(state.has_modified_state());
  ASSERT_EQ(ModuleState::MODULE_STATE_MODIFIED, state.modified_state());
  ASSERT_EQ(static_cast<int>(kModificationSize), num_bytes_different);

  // Modifications across the relocs should have been coalesced into one.
  ASSERT_EQ(1, state.modification_size());
  ASSERT_EQ(static_cast<int>(kModificationSize),
            state.modification(0).byte_count());
  ASSERT_EQ(static_cast<size_t>(kModificationSize),
            state.modification(0).modified_bytes().size());
  EXPECT_EQ(mem_code_data.first(kModificationSize),
            base::as_byte_span(state.modification(0).modified_bytes()));
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
  ScopedModuleModifier<1> mod(GetCodeAfterExport(kTestExportName).first<1>());
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
  ScopedModuleModifier<1> mod2(*GetCodeAfterExport(kTestDllMainExportName)
                                    .subspan(1, 1)
                                    .to_fixed_extent<1>());
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
  base::span<const uint8_t> mem_code_data;
  base::span<const uint8_t> disk_code_data;
  ASSERT_TRUE(GetCodeSpans(*mem_peimage_ptr_, disk_dll_handle_.bytes(),
                           mem_code_data, disk_code_data));

  ScopedModuleModifier<1> mod3(mem_code_data.last<1>());

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
