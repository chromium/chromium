// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/dbus/dbus_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/chromeos/policy/fuzzer/policy_fuzzer.pb.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_value_map.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace policy {

namespace {

struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    base::CommandLine::Init(0, nullptr);
    CHECK(scoped_temp_dir.CreateUniqueTempDir());
    CHECK(base::PathService::Override(chrome::DIR_USER_DATA,
                                      scoped_temp_dir.GetPath()));
    CHECK(base::i18n::InitializeICU());

    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path =
        base::PathService::CheckedGet(ui::UI_TEST_PAK);
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::FilePath pak_path = base::PathService::CheckedGet(base::DIR_MODULE);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("components_tests_resources.pak"),
        ui::SCALE_FACTOR_NONE);
  }

  ~Environment() { ui::ResourceBundle::CleanupSharedInstance(); }

  base::ScopedTempDir scoped_temp_dir;
  base::AtExitManager exit_manager;
};

struct PerInputEnvironment {
  PerInputEnvironment() {
    policy_handler_list = BuildHandlerList(GetChromeSchema());
    chromeos::InitializeDBus();
    chromeos::InitializeFeatureListDependentDBus();
  }

  ~PerInputEnvironment() {
    chromeos::ShutdownDBus();
    chromeos::InstallAttributes::Shutdown();
    chromeos::DeviceSettingsService::Shutdown();
  }

  base::test::TaskEnvironment task_environment;
  std::unique_ptr<ConfigurationPolicyHandlerList> policy_handler_list;
};

void CheckPolicyToPrefTranslation(const PolicyMap& policy_map,
                                  const PerInputEnvironment& per_input_env) {
  PrefValueMap prefs;
  PolicyErrorMap errors;
  PoliciesSet deprecated_policies;
  PoliciesSet future_policies;
  per_input_env.policy_handler_list->ApplyPolicySettings(
      policy_map, &prefs, &errors, &deprecated_policies, &future_policies);
}

void CheckPolicyToCrosSettingsTranslation(
    const enterprise_management::ChromeDeviceSettingsProto&
        chrome_device_settings) {
  PrefValueMap cros_settings_prefs;
  chromeos::DeviceSettingsProvider::DecodePolicies(chrome_device_settings,
                                                   &cros_settings_prefs);

  for (const auto& it : cros_settings_prefs) {
    const std::string& pref_name = it.first;
    CHECK(chromeos::DeviceSettingsProvider::IsDeviceSetting(pref_name));
  }
}

}  // namespace

DEFINE_PROTO_FUZZER(const PolicyFuzzerProto& proto) {
  static Environment env;
  PerInputEnvironment per_input_env;

  if (proto.has_chrome_device_settings()) {
    const enterprise_management::ChromeDeviceSettingsProto&
        chrome_device_settings = proto.chrome_device_settings();
    base::WeakPtr<ExternalDataManager> data_manager;
    PolicyMap policy_map;
    DecodeDevicePolicy(chrome_device_settings, data_manager, &policy_map);

    for (const auto& it : policy_map) {
      const std::string& policy_name = it.first;
      const PolicyMap::Entry& entry = it.second;
      CHECK(entry.value()) << "Policy " << policy_name << " has an empty value";
      CHECK_EQ(entry.scope, POLICY_SCOPE_MACHINE)
          << "Policy " << policy_name << " has not machine scope";
    }

    CheckPolicyToPrefTranslation(policy_map, per_input_env);
    CheckPolicyToCrosSettingsTranslation(chrome_device_settings);
  }

  if (proto.has_cloud_policy_settings()) {
    const enterprise_management::CloudPolicySettings& cloud_policy_settings =
        proto.cloud_policy_settings();
    base::WeakPtr<CloudExternalDataManager> cloud_data_manager;
    PolicyMap policy_map;
    DecodeProtoFields(cloud_policy_settings, cloud_data_manager,
                      PolicySource::POLICY_SOURCE_CLOUD,
                      PolicyScope::POLICY_SCOPE_USER, &policy_map);

    for (const auto& it : policy_map) {
      const std::string& policy_name = it.first;
      const PolicyMap::Entry& entry = it.second;
      CHECK(entry.value()) << "Policy " << policy_name << " has an empty value";
      CHECK_EQ(entry.scope, POLICY_SCOPE_USER)
          << "Policy " << policy_name << " has not user scope";
    }

    CheckPolicyToPrefTranslation(policy_map, per_input_env);
  }
}

}  // namespace policy
