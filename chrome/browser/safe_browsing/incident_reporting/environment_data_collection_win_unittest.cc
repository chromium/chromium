// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/environment_data_collection_win.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/safe_browsing/download_protection/path_sanitizer.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

// Returns true if a dll with filename |dll_name| is found in |process_report|,
// providing a copy of it in |result|.
bool GetProcessReportDll(
    const ClientIncidentReport_EnvironmentData_Process& process_report,
    const base::FilePath& dll_name,
    ClientIncidentReport_EnvironmentData_Process_Dll* result) {
  for (const auto& dll : process_report.dll()) {
    if (base::FilePath::FromUTF8Unsafe(dll.path()).BaseName() == dll_name) {
      result->CopyFrom(dll);
      return true;
    }
  }
  return false;
}

// Look through dll entries and check for the presence of the LSP feature for
// |dll_path|.
bool DllEntryContainsLspFeature(
    const ClientIncidentReport_EnvironmentData_Process& process_report,
    const std::string& dll_path) {
  for (const auto& dll : process_report.dll()) {
    if (dll.path() == dll_path &&
        base::Contains(dll.feature(),
                       ClientIncidentReport_EnvironmentData_Process_Dll::LSP)) {
      // LSP feature found.
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(SafeBrowsingEnvironmentDataCollectionWinTest, CollectDlls) {
  // This test will check if the CollectDlls method works by loading
  // a dll and then checking if we can find it within the process report.
  // Pick msvidc32.dll as it is present from WinXP to Win8 and yet rarely used.
  // msvidc32.dll exists in both 32 and 64 bit versions.
  base::FilePath msvdc32_dll(L"msvidc32.dll");

  ClientIncidentReport_EnvironmentData_Process process_report;
  CollectDlls(&process_report);

  ClientIncidentReport_EnvironmentData_Process_Dll dll;
  ASSERT_FALSE(GetProcessReportDll(process_report, msvdc32_dll, &dll));

  // Redo the same verification after loading a new dll.
  base::ScopedNativeLibrary library(msvdc32_dll);

  process_report.clear_dll();
  CollectDlls(&process_report);

  ASSERT_TRUE(GetProcessReportDll(process_report, msvdc32_dll, &dll));
  ASSERT_TRUE(dll.has_image_headers());
}

TEST(SafeBrowsingEnvironmentDataCollectionWinTest, RecordLspFeature) {
  net::EnsureWinsockInit();

  // Populate our incident report with loaded modules.
  ClientIncidentReport_EnvironmentData_Process process_report;
  CollectDlls(&process_report);

  // We'll test RecordLspFeatures against a real dll registered as a LSP. All
  // dll paths are expected to be lowercase in the process report.
  std::string lsp = "c:\\windows\\system32\\mswsock.dll";
  int base_address = 0x77770000;
  int length = 0x180000;

  RecordLspFeature(&process_report);

  // Return successfully if LSP feature is found.
  if (DllEntryContainsLspFeature(process_report, lsp))
    return;

  // |lsp| was not already loaded into the current process. Manually add it
  // to the process report so that it will get marked as a LSP.
  ClientIncidentReport_EnvironmentData_Process_Dll* dll =
      process_report.add_dll();
  dll->set_path(lsp);
  dll->set_base_address(base_address);
  dll->set_length(length);

  RecordLspFeature(&process_report);

  // Return successfully if LSP feature is found.
  if (DllEntryContainsLspFeature(process_report, lsp))
    return;

  FAIL() << "No LSP feature found for " << lsp;
}

#if !defined(_WIN64)
TEST(SafeBrowsingEnvironmentDataCollectionWinTest, VerifyLoadedModules) {
  //  Load the test modules.
  std::vector<base::ScopedNativeLibrary> test_dlls(kTestDllNamesCount);
  for (size_t i = 0; i < kTestDllNamesCount; ++i)
    test_dlls[i] = base::ScopedNativeLibrary(base::FilePath(kTestDllNames[i]));

  // Edit the first byte of the function exported by the first module. Calling
  // GetModuleHandle so we do not increment the library ref count.
  HMODULE module_handle = GetModuleHandle(kTestDllNames[0]);
  ASSERT_NE(reinterpret_cast<HANDLE>(NULL), module_handle);
  uint8_t* export_addr = reinterpret_cast<uint8_t*>(
      GetProcAddress(module_handle, kTestExportName));
  ASSERT_NE(reinterpret_cast<uint8_t*>(NULL), export_addr);

  uint8_t new_val = (*export_addr) + 1;
  SIZE_T bytes_written = 0;
  WriteProcessMemory(GetCurrentProcess(),
                     export_addr,
                     reinterpret_cast<void*>(&new_val),
                     1,
                     &bytes_written);
  ASSERT_EQ(1u, bytes_written);

  ClientIncidentReport_EnvironmentData_Process process_report;
  CollectModuleVerificationData(kTestDllNames, kTestDllNamesCount,
                                &process_report);

  // CollectModuleVerificationData should return the single modified module and
  // its modified export. The other module, being unmodified, is omitted from
  // the returned list of modules.
  // AddressSanitizer build is special though, as it patches the code at
  // startup, which makes every single module modified and introduces extra
  // exports.
  ASSERT_LE(1, process_report.module_state_size());
#if !defined(ADDRESS_SANITIZER)
  EXPECT_EQ(1, process_report.module_state_size());
  EXPECT_EQ(1, process_report.module_state(0).modification_size());
  EXPECT_TRUE(process_report.module_state(0).modification(0).has_export_name());
#endif

  EXPECT_EQ(base::WideToUTF8(kTestDllNames[0]),
            process_report.module_state(0).name());
  EXPECT_EQ(ClientIncidentReport_EnvironmentData_Process_ModuleState::
                MODULE_STATE_MODIFIED,
            process_report.module_state(0).modified_state());
  // See comment above about AddressSantizier.
#if !defined(ADDRESS_SANITIZER)
  EXPECT_EQ(std::string(kTestExportName),
            process_report.module_state(0).modification(0).export_name());
#endif
}
#endif  // _WIN64

TEST(SafeBrowsingEnvironmentDataCollectionWinTest, CollectRegistryData) {
  // Ensure that all values and subkeys from the specified registry keys are
  // correctly stored in the report.
  registry_util::RegistryOverrideManager override_manager;
  ASSERT_NO_FATAL_FAILURE(override_manager.OverrideRegistry(HKEY_CURRENT_USER));

  const wchar_t kRootKey[] = L"Software\\TestKey";
  const RegistryKeyInfo kRegKeysToCollect[] = {
      {HKEY_CURRENT_USER, kRootKey},
  };
  const wchar_t kSubKey[] = L"SubKey";

  // Check that if the key is not there, then the proto is left empty.
  google::protobuf::RepeatedPtrField<
      ClientIncidentReport_EnvironmentData_OS_RegistryKey> registry_data_pb;
  CollectRegistryData(kRegKeysToCollect, 1, &registry_data_pb);
  EXPECT_EQ(0, registry_data_pb.size());

  base::win::RegKey key(
      HKEY_CURRENT_USER, kRootKey,
      KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_READ | KEY_CREATE_SUB_KEY);

  const wchar_t kStringValueName[] = L"StringValue";
  const wchar_t kDWORDValueName[] = L"DWORDValue";
  const wchar_t kStringData[] = L"string data";
  const DWORD kDWORDData = 0xdeadbeef;

  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kStringValueName, kStringData));

  registry_data_pb.Clear();
  CollectRegistryData(kRegKeysToCollect, 1, &registry_data_pb);

  // Expect 1 registry key, 1 value.
  EXPECT_EQ(1, registry_data_pb.size());
  EXPECT_EQ("HKEY_CURRENT_USER\\Software\\TestKey",
            registry_data_pb.Get(0).name());
  EXPECT_EQ(1, registry_data_pb.Get(0).value_size());
  EXPECT_EQ(base::WideToUTF8(kStringValueName),
            registry_data_pb.Get(0).value(0).name());
  EXPECT_EQ(static_cast<uint32_t>(REG_SZ),
            registry_data_pb.Get(0).value(0).type());
  const char* value_data = registry_data_pb.Get(0).value(0).data().c_str();
  EXPECT_EQ(kStringData,
            std::wstring(reinterpret_cast<const wchar_t*>(&value_data[0])));

  // Add another value.
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kDWORDValueName, kDWORDData));

  registry_data_pb.Clear();
  CollectRegistryData(kRegKeysToCollect, 1, &registry_data_pb);

  // Expect 1 registry key, 2 values.
  EXPECT_EQ(1, registry_data_pb.size());
  EXPECT_EQ(2, registry_data_pb.Get(0).value_size());

  // Add a subkey.
  const wchar_t kTestValueName[] = L"TestValue";
  const wchar_t kTestData[] = L"test data";

  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(kSubKey, KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kRootKey, KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.OpenKey(kSubKey, KEY_READ | KEY_SET_VALUE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kTestValueName, kTestData));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kRootKey, KEY_READ));

  registry_data_pb.Clear();
  CollectRegistryData(kRegKeysToCollect, 1, &registry_data_pb);

  // Expect 1 subkey, 1 value.
  EXPECT_EQ(1, registry_data_pb.Get(0).key_size());
  const ClientIncidentReport_EnvironmentData_OS_RegistryKey& subkey_pb =
      registry_data_pb.Get(0).key(0);
  EXPECT_EQ(base::WideToUTF8(kSubKey), subkey_pb.name());
  EXPECT_EQ(1, subkey_pb.value_size());
  EXPECT_EQ(base::WideToUTF8(kTestValueName), subkey_pb.value(0).name());
  value_data = subkey_pb.value(0).data().c_str();
  EXPECT_EQ(kTestData,
            std::wstring(reinterpret_cast<const wchar_t*>(&value_data[0])));
  EXPECT_EQ(static_cast<uint32_t>(REG_SZ), subkey_pb.value(0).type());
}

TEST(SafeBrowsingEnvironmentDataCollectionWinTest,
     CollectDomainEnrollmentData) {
  // The test may or may not be running on a domain-enrolled machine, so all we
  // can check is that some value is filled in.
  ClientIncidentReport_EnvironmentData_OS os_data;
  CollectDomainEnrollmentData(&os_data);
  EXPECT_TRUE(os_data.has_is_enrolled_to_domain());
}

}  // namespace safe_browsing
