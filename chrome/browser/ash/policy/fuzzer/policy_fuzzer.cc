// Copyright 2019 The Chromium Authors
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
#include "base/syslog_logging.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/ash/dbus/ash_dbus_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_decoder.h"
#include "chrome/browser/ash/policy/fuzzer/policy_fuzzer.pb.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/attestation/attestation_features.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace policy {

namespace {

constexpr logging::LogSeverity kLogSeverity = logging::LOGGING_FATAL;

// A log handler that discards messages whose severity is lower than the
// threshold. It's needed in order to suppress unneeded syslog logging (which by
// default is exempt from the level set by `logging::SetMinLogLevel()`).
bool VoidifyingLogHandler(int severity,
                          const char* /*file*/,
                          int /*line*/,
                          size_t /*message_start*/,
                          const std::string& /*str*/) {
  return severity < kLogSeverity;
}

struct Environment {
  Environment() {
    // Discard all log messages, including the syslog ones, below the threshold.
    logging::SetMinLogLevel(kLogSeverity);
    logging::SetSyslogLoggingForTesting(/*logging_enabled=*/false);
    logging::SetLogMessageHandler(&VoidifyingLogHandler);

    base::CommandLine::Init(0, nullptr);
    TestTimeouts::Initialize();
    CHECK(scoped_temp_dir.CreateUniqueTempDir());
    CHECK(base::PathService::Override(chrome::DIR_USER_DATA,
                                      scoped_temp_dir.GetPath()));
    CHECK(base::i18n::InitializeICU());

    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path =
        base::PathService::CheckedGet(ui::UI_TEST_PAK);
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::FilePath pak_path = base::PathService::CheckedGet(base::DIR_ASSETS);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("components_tests_resources.pak"),
        ui::kScaleFactorNone);
  }

  ~Environment() { ui::ResourceBundle::CleanupSharedInstance(); }

  base::ScopedTempDir scoped_temp_dir;
  base::AtExitManager exit_manager;
};

struct PerInputEnvironment {
  PerInputEnvironment() {
    policy_handler_list = BuildHandlerList(GetChromeSchema());
    ash::InitializeDBus();
    ash::InitializeFeatureListDependentDBus();
  }

  ~PerInputEnvironment() {
    ash::ShutdownDBus();
    ash::InstallAttributes::Shutdown();
    ash::DeviceSettingsService::Shutdown();
    ash::attestation::AttestationFeatures::Shutdown();
  }

  base::test::TaskEnvironment task_environment;
  std::unique_ptr<ConfigurationPolicyHandlerList> policy_handler_list;
};

void CheckPolicyMap(const PolicyMap& policy_map,
                    PolicyScope expected_policy_scope,
                    bool expected_is_device_policy) {
  for (const auto& it : policy_map) {
    const std::string& policy_name = it.first;
    const PolicyMap::Entry& entry = it.second;
    CHECK(entry.value_unsafe())
        << "Policy " << policy_name << " has an empty value";
    CHECK_EQ(entry.scope, expected_policy_scope)
        << "Policy " << policy_name << " has wrong scope";

    const PolicyDetails* policy_details = GetChromePolicyDetails(policy_name);
    CHECK(policy_details) << "Policy " << policy_name
                          << " has no policy details";
    if (expected_is_device_policy) {
      CHECK_EQ(policy_details->scope, kDevice)
        << "Policy " << policy_name << " should be device policy";
    } else {
      CHECK_NE(policy_details->scope, kDevice)
        << "Policy " << policy_name << " should not be device policy";
    }
  }
}

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
  ash::DeviceSettingsProvider::DecodePolicies(chrome_device_settings,
                                              &cros_settings_prefs);

  for (const auto& it : cros_settings_prefs) {
    const std::string& pref_name = it.first;
    CHECK(ash::DeviceSettingsProvider::IsDeviceSetting(pref_name));
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

    CheckPolicyMap(policy_map, POLICY_SCOPE_MACHINE,
                   /*expected_is_device_policy=*/true);
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
                      PolicyScope::POLICY_SCOPE_USER, &policy_map,
                      PolicyPerProfileFilter::kAny);

    CheckPolicyMap(policy_map, POLICY_SCOPE_USER,
                   /*expected_is_device_policy=*/false);
    CheckPolicyToPrefTranslation(policy_map, per_input_env);
  }
}

}  // namespace policy
