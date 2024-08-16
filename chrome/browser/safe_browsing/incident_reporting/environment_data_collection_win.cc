// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/environment_data_collection_win.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <set>
#include <string>

#include "base/enterprise_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_common.h"
#include "chrome/browser/net/service_providers_win.h"
#include "chrome/browser/safe_browsing/download_protection/path_sanitizer.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/variations/variations_associated_data.h"

namespace safe_browsing {

namespace {

const REGSAM kKeyReadNoNotify = (KEY_READ) & ~(KEY_NOTIFY);

// The modules on which we will run VerifyModule.
constexpr std::array<const wchar_t*, 3> kModulesToVerify = {{
    L"chrome.dll",
    L"chrome_elf.dll",
    L"ntdll.dll",
}};

// The registry keys to collect data from.
const std::array<RegistryKeyInfo, 1> kRegKeysToCollect = {{
    {HKEY_CURRENT_USER, L"Software\\CSAStats"},
}};

// Helper function to convert HKEYs to strings.
std::wstring HKEYToString(HKEY key) {
  DCHECK_EQ(HKEY_CURRENT_USER, key);
  return L"HKEY_CURRENT_USER";
}

// Helper function to extract data from a single registry key (and all of its
// subkeys), recursively.
void CollectRegistryDataForKey(
    const base::win::RegKey& key,
    ClientIncidentReport_EnvironmentData_OS_RegistryKey* key_pb) {
  using RegistryValueProto =
      ClientIncidentReport_EnvironmentData_OS_RegistryValue;
  using RegistryKeyProto = ClientIncidentReport_EnvironmentData_OS_RegistryKey;

  DWORD num_subkeys = 0;
  DWORD max_subkey_name_len = 0;
  DWORD num_values = 0;
  DWORD max_value_name_len = 0;
  DWORD max_value_len = 0;
  LONG result = RegQueryInfoKey(
      key.Handle(), NULL, NULL, NULL, &num_subkeys, &max_subkey_name_len, NULL,
      &num_values, &max_value_name_len, &max_value_len, NULL, NULL);

  DWORD max_name_len = std::max(max_subkey_name_len, max_value_name_len) + 1;
  std::vector<wchar_t> name_buffer(max_name_len);
  // Read the values.
  if (num_values != 0) {
    std::vector<uint8_t> value_buffer(max_value_len != 0 ? max_value_len : 1);
    DWORD name_size = 0;
    DWORD value_type = REG_NONE;
    DWORD value_size = 0;

    std::wstring name_str;
    RegistryValueProto* registry_value;
    for (DWORD i = 0; i < num_values;) {
      name_size = static_cast<DWORD>(name_buffer.size());
      value_size = static_cast<DWORD>(value_buffer.size());
      result = ::RegEnumValue(key.Handle(), i, &name_buffer[0], &name_size,
                              NULL, &value_type, &value_buffer[0], &value_size);
      switch (result) {
        case ERROR_NO_MORE_ITEMS:
          i = num_values;
          break;
        case ERROR_SUCCESS:
          registry_value = key_pb->add_value();
          if (name_size) {
            name_str.assign(&name_buffer[0], name_size);
            registry_value->set_name(base::WideToUTF8(name_str));
          }
          if (value_size) {
            registry_value->set_type(value_type);
            registry_value->set_data(&value_buffer[0], value_size);
          }
          ++i;
          break;
        case ERROR_MORE_DATA:
          if (value_size > value_buffer.size())
            value_buffer.resize(value_size);
          // |name_size| does not include space for the terminating NULL.
          if (name_size + 1 > name_buffer.size())
            name_buffer.resize(name_size + 1);
          break;
        default:
          break;
      }
    }
  }

  // Read the subkeys.
  if (num_subkeys != 0) {
    DWORD name_size = 0;
    std::vector<std::wstring> subkey_names;

    // Get the names of them.
    for (DWORD i = 0; i < num_subkeys;) {
      name_size = static_cast<DWORD>(name_buffer.size());
      result = RegEnumKeyEx(key.Handle(), i, &name_buffer[0], &name_size, NULL,
                            NULL, NULL, NULL);
      switch (result) {
        case ERROR_NO_MORE_ITEMS:
          num_subkeys = i;
          break;
        case ERROR_SUCCESS:
          subkey_names.push_back(std::wstring(&name_buffer[0], name_size));
          ++i;
          break;
        case ERROR_MORE_DATA:
          name_buffer.resize(name_size + 1);
          break;
        default:
          break;
      }
    }

    // Extract the data (if possible).
    base::win::RegKey subkey;
    for (const auto& subkey_name : subkey_names) {
      if (subkey.Open(key.Handle(), subkey_name.c_str(), kKeyReadNoNotify) ==
          ERROR_SUCCESS) {
        RegistryKeyProto* subkey_pb = key_pb->add_key();
        subkey_pb->set_name(base::WideToUTF8(subkey_name));
        CollectRegistryDataForKey(subkey, subkey_pb);
      }
    }
  }
}

}  // namespace

bool CollectDlls(ClientIncidentReport_EnvironmentData_Process* process) {
  // Retrieve the module list.
  std::set<ModuleInfo> loaded_modules;
  if (!GetLoadedModules(&loaded_modules))
    return false;

  // Sanitize path of each module and add it to the incident report along with
  // its headers.
  PathSanitizer path_sanitizer;
  scoped_refptr<BinaryFeatureExtractor> feature_extractor(
      new BinaryFeatureExtractor());
  for (const auto& module : loaded_modules) {
    base::FilePath dll_path(module.name);
    base::FilePath sanitized_path(dll_path);
    path_sanitizer.StripHomeDirectory(&sanitized_path);

    ClientIncidentReport_EnvironmentData_Process_Dll* dll = process->add_dll();
    dll->set_path(
        base::UTF16ToUTF8(base::i18n::ToLower(sanitized_path.AsUTF16Unsafe())));
    dll->set_base_address(module.base_address);
    dll->set_length(module.size);
    // TODO(grt): Consider skipping this for valid system modules.
    if (!feature_extractor->ExtractImageFeatures(
            dll_path,
            BinaryFeatureExtractor::kOmitExports,
            dll->mutable_image_headers(),
            nullptr /* signed_data */)) {
      dll->clear_image_headers();
    }
  }

  return true;
}

void RecordLspFeature(ClientIncidentReport_EnvironmentData_Process* process) {
  WinsockLayeredServiceProviderList lsp_list;
  GetWinsockLayeredServiceProviders(&lsp_list);

  // For each LSP, we extract and sanitize the path.
  PathSanitizer path_sanitizer;
  std::set<std::wstring> lsp_paths;
  for (size_t i = 0; i < lsp_list.size(); ++i) {
    auto expanded_path =
        base::win::ExpandEnvironmentVariables(lsp_list[i].path);
    base::FilePath lsp_path(expanded_path.value_or(lsp_list[i].path));
    path_sanitizer.StripHomeDirectory(&lsp_path);
    lsp_paths.insert(
        base::UTF16ToWide(base::i18n::ToLower(lsp_path.AsUTF16Unsafe())));
  }

  // Look for a match between LSPs and loaded dlls.
  for (int i = 0; i < process->dll_size(); ++i) {
    if (lsp_paths.count(base::UTF8ToWide(process->dll(i).path()))) {
      process->mutable_dll(i)
          ->add_feature(ClientIncidentReport_EnvironmentData_Process_Dll::LSP);
    }
  }
}

void CollectModuleVerificationData(
    base::span<const wchar_t* const> modules_to_verify,
    ClientIncidentReport_EnvironmentData_Process* process) {
#if !defined(_WIN64)
  using ModuleState = ClientIncidentReport_EnvironmentData_Process_ModuleState;

  for (const wchar_t* const module_name : modules_to_verify) {
    auto module_state = std::make_unique<ModuleState>();

    int num_bytes_different = 0;
    VerifyModule(module_name, module_state.get(), &num_bytes_different);

    if (module_state->modified_state() == ModuleState::MODULE_STATE_UNMODIFIED)
      continue;

    process->mutable_module_state()->AddAllocated(module_state.release());
  }
#endif  // _WIN64
}

void CollectRegistryData(
    base::span<const RegistryKeyInfo> keys_to_collect,
    google::protobuf::RepeatedPtrField<
        ClientIncidentReport_EnvironmentData_OS_RegistryKey>* key_data) {
  using RegistryKeyProto = ClientIncidentReport_EnvironmentData_OS_RegistryKey;
  for (const RegistryKeyInfo& key_info : keys_to_collect) {
    base::win::RegKey reg_key(key_info.rootkey, key_info.subkey,
                              kKeyReadNoNotify);
    if (reg_key.Valid()) {
      RegistryKeyProto* regkey_pb = key_data->Add();
      std::wstring rootkey_name = HKEYToString(key_info.rootkey);
      rootkey_name += L"\\";
      rootkey_name += key_info.subkey;
      regkey_pb->set_name(base::WideToUTF8(rootkey_name));
      CollectRegistryDataForKey(reg_key, regkey_pb);
    }
  }
}

void CollectDomainEnrollmentData(
    ClientIncidentReport_EnvironmentData_OS* os_data) {
  os_data->set_is_enrolled_to_domain(base::IsEnterpriseDevice());
}

void CollectPlatformProcessData(
    ClientIncidentReport_EnvironmentData_Process* process) {
  CollectDlls(process);
  RecordLspFeature(process);
  CollectModuleVerificationData(kModulesToVerify, process);
}

void CollectPlatformOSData(ClientIncidentReport_EnvironmentData_OS* os_data) {
  CollectRegistryData(kRegKeysToCollect, os_data->mutable_registry_key());
  CollectDomainEnrollmentData(os_data);
}
}  // namespace safe_browsing
