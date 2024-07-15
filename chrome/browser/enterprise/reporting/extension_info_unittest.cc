// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_info.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"

using extensions::mojom::ManifestLocation;

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const char kName[] = "extension";
const char kId[] = "abcdefghijklmnoabcdefghijklmnoab";
const char kId2[] = "abcdefghijklmnoabcdefghijklmnoac";
const char kVersion[] = "1.0.0";
const char kDescription[] = "an extension description.";
const char kHomepage[] = "https://foo.com/extension";
const char kAPIPermission1[] = "alarms";
const char kAPIPermission2[] = "idle";
const char kHostPermission[] = "*://*.example.com/*";
const char kAPIPermissionOptional[] = "storage";
const char kHostPermissionOptional[] = "https://www.example2.com/*";
const char kAppLaunchUrl[] = "https://www.example.com/";
const int kManifestVersion = 2;

}  // namespace

class ExtensionInfoTest : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service()->Init();
  }

  scoped_refptr<const extensions::Extension> BuildExtension(
      const std::string& id = kId,
      ManifestLocation location = ManifestLocation::kUnpacked,
      bool is_app = false,
      bool from_webstore = false) {
    extensions::ExtensionBuilder extensionBuilder(
        kName, (is_app ? extensions::ExtensionBuilder::Type::PLATFORM_APP
                       : extensions::ExtensionBuilder::Type::EXTENSION));
    extensionBuilder.SetID(id)
        .SetVersion(kVersion)
        .SetManifestVersion(kManifestVersion)
        .SetManifestKey(extensions::manifest_keys::kDescription, kDescription)
        .SetManifestKey(extensions::manifest_keys::kHomepageURL, kHomepage)
        .SetLocation(location)
        .AddAPIPermission(kAPIPermission1)
        .AddAPIPermission(kAPIPermission2)
        .AddHostPermission(kHostPermission)
        .AddOptionalAPIPermission(kAPIPermissionOptional)
        .AddOptionalHostPermission(kHostPermissionOptional);
    if (is_app) {
      extensionBuilder.SetManifestPath("app.launch.web_url", kAppLaunchUrl);
    }
    if (from_webstore) {
      extensionBuilder.AddFlags(extensions::Extension::FROM_WEBSTORE);
    }
    auto extension = extensionBuilder.Build();
    service()->AddExtension(extension.get());
    return extension;
  }
};

TEST_F(ExtensionInfoTest, ExtensionReport) {
  auto extension = BuildExtension();

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(1, info.extensions_size());
  const em::Extension actual_extension_report = info.extensions(0);

  EXPECT_EQ(kId, actual_extension_report.id());
  EXPECT_EQ(kName, actual_extension_report.name());
  EXPECT_EQ(kVersion, actual_extension_report.version());
  EXPECT_EQ(kDescription, actual_extension_report.description());
  EXPECT_EQ(kManifestVersion, actual_extension_report.manifest_version());

  EXPECT_EQ(em::Extension_ExtensionType_TYPE_EXTENSION,
            actual_extension_report.app_type());
  EXPECT_EQ(em::Extension_InstallType_TYPE_DEVELOPMENT,
            actual_extension_report.install_type());

  EXPECT_EQ(kHomepage, actual_extension_report.homepage_url());

  EXPECT_TRUE(actual_extension_report.enabled());
  EXPECT_FALSE(actual_extension_report.from_webstore());

  EXPECT_EQ(3, actual_extension_report.permissions_size());
  EXPECT_EQ(kAPIPermission1, actual_extension_report.permissions(0));
  EXPECT_EQ(kAPIPermission2, actual_extension_report.permissions(1));
  EXPECT_EQ(kAPIPermissionOptional, actual_extension_report.permissions(2));
  EXPECT_EQ(2, actual_extension_report.host_permissions_size());
  EXPECT_EQ(kHostPermission, actual_extension_report.host_permissions(0));
  EXPECT_EQ(kHostPermissionOptional,
            actual_extension_report.host_permissions(1));
}

TEST_F(ExtensionInfoTest, MultipleExtensions) {
  auto extension1 = BuildExtension(kId);
  auto extension2 = BuildExtension(kId2);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(2, info.extensions_size());
  EXPECT_EQ(kId, info.extensions(0).id());
  EXPECT_EQ(kId2, info.extensions(1).id());
}

TEST_F(ExtensionInfoTest, ExtensionDisabled) {
  auto extension = BuildExtension();
  service()->DisableExtension(kId,
                              extensions::disable_reason::DISABLE_USER_ACTION);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(1, info.extensions_size());

  EXPECT_FALSE(info.extensions(0).enabled());
}

TEST_F(ExtensionInfoTest, ExtensionTerminated) {
  auto extension = BuildExtension();
  service()->TerminateExtension(kId);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(1, info.extensions_size());

  EXPECT_FALSE(info.extensions(0).enabled());
}

TEST_F(ExtensionInfoTest, ExtensionBlocked) {
  auto extension = BuildExtension();
  service()->BlockAllExtensions();

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(0, info.extensions_size());
}

TEST_F(ExtensionInfoTest, ExtensionBlocklisted) {
  auto extension = BuildExtension();
  service()->BlocklistExtensionForTest(kId);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(0, info.extensions_size());
}

TEST_F(ExtensionInfoTest, ComponentExtension) {
  auto extension1 = BuildExtension(kId, ManifestLocation::kComponent);
  auto extension2 = BuildExtension(kId2, ManifestLocation::kExternalComponent);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(0, info.extensions_size());
}

TEST_F(ExtensionInfoTest, FromWebstoreFlag) {
  auto extension1 = BuildExtension(kId, ManifestLocation::kUnpacked,
                                   /*is_app=*/false, /*from_webstore=*/false);
  auto extension2 = BuildExtension(kId2, ManifestLocation::kUnpacked,
                                   /*is_app=*/false, /*from_webstore=*/true);

  em::ChromeUserProfileInfo info;
  AppendExtensionInfoIntoProfileReport(profile(), &info);

  EXPECT_EQ(2, info.extensions_size());
  EXPECT_FALSE(info.extensions(0).from_webstore());
  EXPECT_TRUE(info.extensions(1).from_webstore());
}

}  // namespace enterprise_reporting
