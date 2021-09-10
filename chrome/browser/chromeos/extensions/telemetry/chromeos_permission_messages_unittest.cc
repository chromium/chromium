// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

using extensions::PermissionsParser;
using extensions::mojom::ManifestLocation;

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
      std::unique_ptr<base::ListValue> required_permissions) {
    app_ =
        extensions::ExtensionBuilder("Test ChromeOS System Extension")
            .SetManifestKey("chromeos_system_extension",
                            extensions::DictionaryBuilder().Build())
            .SetManifestKey("permissions", std::move(required_permissions))
            .SetManifestKey("manifest_version", 3)
            .SetID("gogonhoemckpdpadfnjnpgbjpbjnodgc")  // only allowlisted id
            .SetLocation(ManifestLocation::kInternal)
            .Build();

    env_.GetExtensionService()->AddExtension(app_.get());
  }

  std::vector<std::u16string> GetRequiredPermissionsMessages() {
    std::vector<std::u16string> messages;
    for (const extensions::PermissionMessage& msg :
         message_provider_->GetPermissionMessages(
             message_provider_->GetAllPermissionIDs(
                 PermissionsParser::GetRequiredPermissions(app_.get()),
                 app_->GetType()))) {
      messages.push_back(msg.message());
    }
    return messages;
  }

 private:
  extensions::TestExtensionEnvironment env_;
  std::unique_ptr<extensions::ChromePermissionMessageProvider>
      message_provider_;
  scoped_refptr<const extensions::Extension> app_;
};

TEST_F(ChromeOSPermissionMessageUnittest, OsTelemetryMessage) {
  CreateAndInstallExtensionWithPermissions(
      extensions::ListBuilder().Append("os.telemetry").Build());

  ASSERT_EQ(1U, GetRequiredPermissionsMessages().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_TELEMETRY),
            GetRequiredPermissionsMessages()[0]);
}

}  // namespace chromeos
