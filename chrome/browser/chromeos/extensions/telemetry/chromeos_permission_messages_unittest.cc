// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/value_builder.h"
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
const std::u16string kTelemetryPermissionMessage =
    u"Read ChromeOS device information and device data";
const std::u16string kTelemetrySerialNumberPermissionMessage =
    u"Read ChromeOS device and component serial numbers";
const std::u16string kTelemetryNetworkInformationPermissionMessage =
    u"Read ChromeOS network information";

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
               .SetManifestKey(
                   "externally_connectable",
                   extensions::DictionaryBuilder()
                       .Set("matches",
                            extensions::ListBuilder()
                                .Append("*://googlechromelabs.github.io/*")
                                .Build())
                       .Build())
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

TEST_F(ChromeOSPermissionMessageUnittest, OsDiagnosticsMessage) {
  CreateAndInstallExtensionWithPermissions(
      extensions::ListBuilder().Append("os.diagnostics").Build(),
      base::Value::List());

  ASSERT_EQ(0U, optional_permissions().size());
  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(kDiagnosticsPermissionMessage, required_permissions()[0]);
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kDiagnosticsPermissionMessage, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsTelemetryMessage) {
  CreateAndInstallExtensionWithPermissions(
      extensions::ListBuilder().Append("os.telemetry").Build(),
      base::Value::List());

  ASSERT_EQ(0U, optional_permissions().size());
  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(kTelemetryPermissionMessage, required_permissions()[0]);
  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(kTelemetryPermissionMessage, active_permissions()[0]);
}

TEST_F(ChromeOSPermissionMessageUnittest, OsTelemetrySerialNumber) {
  CreateAndInstallExtensionWithPermissions(
      base::Value::List(),
      extensions::ListBuilder().Append("os.telemetry.serial_number").Build());

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
      extensions::ListBuilder().Append("os.telemetry.network_info").Build());

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

}  // namespace chromeos
