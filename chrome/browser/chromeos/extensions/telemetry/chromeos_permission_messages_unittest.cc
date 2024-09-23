// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using extensions::PermissionSet;
using extensions::PermissionsParser;
using extensions::mojom::ManifestLocation;

constexpr char kChromeOSSystemExtensionId[] =
    "gogonhoemckpdpadfnjnpgbjpbjnodgc";
const std::u16string kDiagnosticsPermissionMessage =
    u"Run ChromeOS diagnostic tests";
const std::u16string kTelemetryEventsPermissionMessage =
    u"Subscribe to ChromeOS system events";
const std::u16string kTelemetryPermissionMessage =
    u"Read ChromeOS device information and data";
const std::u16string kTelemetrySerialNumberPermissionMessage =
    u"Read ChromeOS device and component serial numbers";
const std::u16string kTelemetryNetworkInformationPermissionMessage =
    u"Read ChromeOS network information";
const std::u16string kAttachedDeviceInfo =
    u"Read attached devices information and data";
const std::u16string kBluetoothPeripheralsInfo =
    u"Read Bluetooth peripherals information and data";
const std::u16string kManagementAudio = u"Manage ChromeOS audio settings";
const std::u16string kDiagnosticsNetworkInfoForMlab =
    u"Collect IP address and network measurement results for Measurement Lab, "
    u"according to their privacy policy (measurementlab.net/privacy)";
}  // namespace

// Tests that ChromePermissionMessageProvider provides not only correct, but
// meaningful permission messages that coalesce correctly where appropriate.
// The following tests target "chromeos_system_extension" related permissions
// (e.g. os.telemetry)
class ChromeOSPermissionMessageUnittest : public testing::Test {
 public:
  ChromeOSPermissionMessageUnittest()
      : message_provider_(new extensions::ChromePermissionMessageProvider()) {}
  ChromeOSPermissionMessageUnittest(const ChromeOSPermissionMessageUnittest&) =
      delete;
  ChromeOSPermissionMessageUnittest& operator=(
      const ChromeOSPermissionMessageUnittest&) = delete;
  ~ChromeOSPermissionMessageUnittest() override {}

 protected:
  void CreateAndInstallExtensionWithPermissions(
      base::Value::List required_permissions,
      base::Value::List optional_permissions) {
    app_ = extensions::ExtensionBuilder("Test ChromeOS System Extension")
               .SetManifestVersion(3)
               .SetManifestKey("chromeos_system_extension", base::Value::Dict())
               .SetManifestKey("permissions", std::move(required_permissions))
               .SetManifestKey("optional_permissions",
                               std::move(optional_permissions))
               .SetManifestKey("externally_connectable",
                               base::Value::Dict().Set(
                                   "matches",
                                   base::Value::List().Append(
                                       "*://googlechromelabs.github.io/*")))
               .SetID(kChromeOSSystemExtensionId)  // only allowlisted id
               .SetLocation(ManifestLocation::kInternal)
               .Build();

    env_.GetExtensionService()->AddExtension(app_.get());
  }

  // Returns the permission messages that would display in the prompt that
  // requests all the optional permissions for the current |app_|.
  std::vector<std::u16string> GetInactiveOptionalPermissionMessages() {
    std::unique_ptr<const PermissionSet> granted_permissions =
        env_.GetExtensionPrefs()->GetGrantedPermissions(app_->id());
    const PermissionSet& optional_permissions =
        PermissionsParser::GetOptionalPermissions(app_.get());
    std::unique_ptr<const PermissionSet> requested_permissions =
        PermissionSet::CreateDifference(optional_permissions,
                                        *granted_permissions);
    return GetMessages(*requested_permissions);
  }

  void GrantOptionalPermissions() {
    extensions::permissions_test_util::
        GrantOptionalPermissionsAndWaitForCompletion(
            env_.profile(), *app_,
            PermissionsParser::GetOptionalPermissions(app_.get()));
  }

  std::vector<std::u16string> active_permissions() {
    return GetMessages(app_->permissions_data()->active_permissions());
  }

  std::vector<std::u16string> required_permissions() {
    return GetMessages(PermissionsParser::GetRequiredPermissions(app_.get()));
  }

  std::vector<std::u16string> optional_permissions() {
    return GetMessages(PermissionsParser::GetOptionalPermissions(app_.get()));
  }

 private:
  std::vector<std::u16string> GetMessages(const PermissionSet& permissions) {
    std::vector<std::u16string> messages;
    for (const extensions::PermissionMessage& msg :
         message_provider_->GetPermissionMessages(
             message_provider_->GetAllPermissionIDs(permissions,
                                                    app_->GetType()))) {
      messages.push_back(msg.message());
    }
    return messages;
  }

  extensions::TestExtensionEnvironment env_;
  std::unique_ptr<extensions::ChromePermissionMessageProvider>
      message_provider_;
  scoped_refptr<const extensions::Extension> app_;
};

TEST_F(ChromeOSPermissionMessageUnittest, OsAttachedDeviceInfo) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(),
      base::Value::List().Append("os.attached_device_info"));

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(kAttachedDeviceInfo, optional_permissions()[0]);
  ASSERT_EQ(1U, GetInactiveOptionalPermissionMessages().size());
  EXPECT_EQ(kAttachedDeviceInfo, GetInactiveOptionalPermissionMessages()[0]);
  EXPECT_EQ(0U, required_permissions().size());
  EXPECT_EQ(0U, active_permissions().size());

  GrantOptionalPermissions();

  EXPECT_EQ(0U, GetInactiveOptionalPermissionMessages().size());
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kAttachedDeviceInfo, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsBluetoothPeripheralsInfo) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(),
      base::Value::List().Append("os.bluetooth_peripherals_info"));

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(kBluetoothPeripheralsInfo, optional_permissions()[0]);
  ASSERT_EQ(1U, GetInactiveOptionalPermissionMessages().size());
  EXPECT_EQ(kBluetoothPeripheralsInfo,
            GetInactiveOptionalPermissionMessages()[0]);
  EXPECT_EQ(0U, required_permissions().size());
  EXPECT_EQ(0U, active_permissions().size());

  GrantOptionalPermissions();

  EXPECT_EQ(0U, GetInactiveOptionalPermissionMessages().size());
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kBluetoothPeripheralsInfo, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsDiagnosticsMessage) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List().Append("os.diagnostics"), base::Value::List());

  ASSERT_EQ(0U, optional_permissions().size());
  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(kDiagnosticsPermissionMessage, required_permissions()[0]);
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kDiagnosticsPermissionMessage, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsDiagnosticsNetworkInfoForMlab) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(),
      base::Value::List().Append("os.diagnostics.network_info_mlab"));

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(kDiagnosticsNetworkInfoForMlab, optional_permissions()[0]);
  ASSERT_EQ(1U, GetInactiveOptionalPermissionMessages().size());
  EXPECT_EQ(kDiagnosticsNetworkInfoForMlab,
            GetInactiveOptionalPermissionMessages()[0]);
  EXPECT_EQ(0U, required_permissions().size());
  EXPECT_EQ(0U, active_permissions().size());

  GrantOptionalPermissions();

  EXPECT_EQ(0U, GetInactiveOptionalPermissionMessages().size());
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kDiagnosticsNetworkInfoForMlab, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsTelemetryMessage) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List().Append("os.telemetry"), base::Value::List());

  ASSERT_EQ(0U, optional_permissions().size());
  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(kTelemetryPermissionMessage, required_permissions()[0]);
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kTelemetryPermissionMessage, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsTelemetrySerialNumber) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(),
      base::Value::List().Append("os.telemetry.serial_number"));

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(kTelemetrySerialNumberPermissionMessage, optional_permissions()[0]);
  ASSERT_EQ(1U, GetInactiveOptionalPermissionMessages().size());
  EXPECT_EQ(kTelemetrySerialNumberPermissionMessage,
            GetInactiveOptionalPermissionMessages()[0]);
  ASSERT_EQ(0U, required_permissions().size());
  ASSERT_EQ(0U, active_permissions().size());

  GrantOptionalPermissions();

  ASSERT_EQ(0U, GetInactiveOptionalPermissionMessages().size());
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kTelemetrySerialNumberPermissionMessage, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsTelemetryNetworkInformation) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(),
      base::Value::List().Append("os.telemetry.network_info"));

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(kTelemetryNetworkInformationPermissionMessage,
            optional_permissions()[0]);
  ASSERT_EQ(1U, GetInactiveOptionalPermissionMessages().size());
  EXPECT_EQ(kTelemetryNetworkInformationPermissionMessage,
            GetInactiveOptionalPermissionMessages()[0]);
  ASSERT_EQ(0U, required_permissions().size());
  ASSERT_EQ(0U, active_permissions().size());

  GrantOptionalPermissions();

  ASSERT_EQ(0U, GetInactiveOptionalPermissionMessages().size());
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kTelemetryNetworkInformationPermissionMessage,
            active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsTelemetryEventsMessage) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(), base::Value::List().Append("os.events"));

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(kTelemetryEventsPermissionMessage, optional_permissions()[0]);
  ASSERT_EQ(1U, GetInactiveOptionalPermissionMessages().size());
  EXPECT_EQ(kTelemetryEventsPermissionMessage,
            GetInactiveOptionalPermissionMessages()[0]);
  EXPECT_EQ(0U, required_permissions().size());
  EXPECT_EQ(0U, active_permissions().size());

  GrantOptionalPermissions();

  EXPECT_EQ(0U, GetInactiveOptionalPermissionMessages().size());
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kTelemetryEventsPermissionMessage, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsManagementAudio) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(), base::Value::List().Append("os.management.audio"));

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(kManagementAudio, optional_permissions()[0]);
  ASSERT_EQ(1U, GetInactiveOptionalPermissionMessages().size());
  EXPECT_EQ(kManagementAudio, GetInactiveOptionalPermissionMessages()[0]);
  EXPECT_EQ(0U, required_permissions().size());
  EXPECT_EQ(0U, active_permissions().size());

  GrantOptionalPermissions();

  EXPECT_EQ(0U, GetInactiveOptionalPermissionMessages().size());
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kManagementAudio, active_permissions()[0]);
}

}  // namespace chromeos
