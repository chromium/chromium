// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/event_initiator_utils.h"

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_download_item.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo;
using extensions::mojom::ManifestLocation;

constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kUninstalledExtensionId[] = "ponmlkjihgfedcbaponmlkjihgfedcba";
constexpr char kExtensionName[] = "Test Extension";
constexpr char kExtensionVersion[] = "1.2.3";

}  // namespace

class EventInitiatorUtilsTest : public testing::Test {
 public:
  // Creates an extension without registering it in `profile_`.
  scoped_refptr<const extensions::Extension> CreateExtension(
      ManifestLocation location,
      int creation_flags) {
    return extensions::ExtensionBuilder(kExtensionName)
        .SetVersion(kExtensionVersion)
        .SetLocation(location)
        .AddFlags(creation_flags)
        .SetID(kExtensionId)
        .Build();
  }

  // Creates an extension and registers it as enabled in `profile_`.
  scoped_refptr<const extensions::Extension> InstallExtension(
      ManifestLocation location,
      int creation_flags) {
    scoped_refptr<const extensions::Extension> extension =
        CreateExtension(location, creation_flags);
    extensions::ExtensionRegistry::Get(&profile_)->AddEnabled(extension);
    return extension;
  }

  // Creates a download initiated by extension `id`. If `attach_profile` is
  // true, `profile_` is attached to the item as its BrowserContext, as
  // DownloadManagerImpl does for real downloads.
  std::unique_ptr<content::FakeDownloadItem> CreateDownloadByExtension(
      const std::string& id,
      const std::string& name = kExtensionName,
      bool attach_profile = true) {
    auto item = std::make_unique<content::FakeDownloadItem>();
    if (attach_profile) {
      content::DownloadItemUtils::AttachInfoForTesting(
          item.get(), &profile_, /*web_contents=*/nullptr);
    }
    new extensions::DownloadedByExtension(item.get(), id, name);
    return item;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(EventInitiatorUtilsTest, NullDownloadReturnsNullopt) {
  EXPECT_FALSE(GetExtensionInitiatorInfo(/*download=*/nullptr).has_value());
}

TEST_F(EventInitiatorUtilsTest, DownloadWithoutExtensionReturnsNullopt) {
  content::FakeDownloadItem item;
  EXPECT_FALSE(GetExtensionInitiatorInfo(&item).has_value());
}

TEST_F(EventInitiatorUtilsTest, EmptyExtensionIdReturnsNullopt) {
  auto item = CreateDownloadByExtension(/*id=*/"");
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
}

TEST_F(EventInitiatorUtilsTest, WebstoreExtensionPopulatesProvenance) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_EQ(kExtensionVersion, extension_info->version());
  EXPECT_EQ(ExtensionInfo::INTERNAL, extension_info->install_location());
  EXPECT_TRUE(extension_info->is_from_store());
}

// The name in ExtensionRegistry may differ from the name snapshotted when the
// download started (e.g. after an update); the registry name wins.
TEST_F(EventInitiatorUtilsTest, PrefersRegistryNameOverSnapshottedName) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownloadByExtension(kExtensionId, "Stale Name");

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionName, extension_info->name());
}

// A self-hosted, policy-installed extension: pushed by the admin, but not from
// the Web Store. (A policy-installed extension whose update URL is the Web
// Store would be created with FROM_WEBSTORE and report `is_from_store` true.)
TEST_F(EventInitiatorUtilsTest, PolicyInstalledExtensionReportsPolicyLocation) {
  InstallExtension(ManifestLocation::kExternalPolicyDownload,
                   extensions::Extension::NO_FLAGS);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(ExtensionInfo::EXTERNAL_POLICY_DOWNLOAD,
            extension_info->install_location());
  ASSERT_TRUE(extension_info->has_is_from_store());
  EXPECT_FALSE(extension_info->is_from_store());
}

// A sideloaded extension can claim a Web Store ID, but not its provenance.
TEST_F(EventInitiatorUtilsTest, UnpackedExtensionIsNotMarkedAsFromStore) {
  InstallExtension(ManifestLocation::kUnpacked,
                   extensions::Extension::NO_FLAGS);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(ExtensionInfo::UNPACKED, extension_info->install_location());
  ASSERT_TRUE(extension_info->has_is_from_store());
  EXPECT_FALSE(extension_info->is_from_store());
}

// An extension may be disabled between starting an action and the event being
// emitted. It must still be fully attributed.
TEST_F(EventInitiatorUtilsTest, DisabledExtensionIsStillAttributed) {
  extensions::ExtensionRegistry::Get(&profile_)->AddDisabled(CreateExtension(
      ManifestLocation::kInternal, extensions::Extension::FROM_WEBSTORE));
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_EQ(kExtensionVersion, extension_info->version());
  EXPECT_EQ(ExtensionInfo::INTERNAL, extension_info->install_location());
  EXPECT_TRUE(extension_info->is_from_store());
}

// An extension that is gone by the time the event is emitted is still
// attributed with its ID and snapshotted name, but with no provenance claims
// attached.
TEST_F(EventInitiatorUtilsTest, UnknownExtensionReportsIdWithoutProvenance) {
  auto item = CreateDownloadByExtension(kUninstalledExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kUninstalledExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());

  // Provenance stays unset so consumers can tell "unknown" from "false".
  EXPECT_FALSE(extension_info->has_version());
  EXPECT_FALSE(extension_info->has_install_location());
  EXPECT_FALSE(extension_info->has_is_from_store());
}

TEST_F(EventInitiatorUtilsTest, EmptySnapshottedNameIsNotSet) {
  auto item = CreateDownloadByExtension(kUninstalledExtensionId, /*name=*/"");

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kUninstalledExtensionId, extension_info->id());
  EXPECT_FALSE(extension_info->has_name());
}

// A download with no BrowserContext attached cannot be looked up in any
// ExtensionRegistry, even if the extension is installed.
TEST_F(EventInitiatorUtilsTest, NoBrowserContextReportsIdAndNameOnly) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownloadByExtension(kExtensionId, kExtensionName,
                                        /*attach_profile=*/false);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_FALSE(extension_info->has_version());
  EXPECT_FALSE(extension_info->has_install_location());
  EXPECT_FALSE(extension_info->has_is_from_store());
}

struct InstallLocationTestCase {
  ManifestLocation manifest_location;
  ExtensionInfo::InstallLocation expected_install_location;
};

class EventInitiatorUtilsInstallLocationTest
    : public EventInitiatorUtilsTest,
      public testing::WithParamInterface<InstallLocationTestCase> {};

TEST_P(EventInitiatorUtilsInstallLocationTest, MapsManifestLocation) {
  InstallExtension(GetParam().manifest_location,
                   extensions::Extension::NO_FLAGS);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(GetParam().expected_install_location,
            extension_info->install_location());
}

// Covers every valid ManifestLocation. kInvalidLocation is excluded because an
// installed extension never has it.
INSTANTIATE_TEST_SUITE_P(
    All,
    EventInitiatorUtilsInstallLocationTest,
    testing::Values(
        InstallLocationTestCase{ManifestLocation::kInternal,
                                ExtensionInfo::INTERNAL},
        InstallLocationTestCase{ManifestLocation::kExternalPref,
                                ExtensionInfo::EXTERNAL_PREF},
        InstallLocationTestCase{ManifestLocation::kExternalRegistry,
                                ExtensionInfo::EXTERNAL_REGISTRY},
        InstallLocationTestCase{ManifestLocation::kUnpacked,
                                ExtensionInfo::UNPACKED},
        InstallLocationTestCase{ManifestLocation::kComponent,
                                ExtensionInfo::COMPONENT},
        InstallLocationTestCase{ManifestLocation::kExternalPrefDownload,
                                ExtensionInfo::EXTERNAL_PREF_DOWNLOAD},
        InstallLocationTestCase{ManifestLocation::kExternalPolicyDownload,
                                ExtensionInfo::EXTERNAL_POLICY_DOWNLOAD},
        InstallLocationTestCase{ManifestLocation::kCommandLine,
                                ExtensionInfo::COMMAND_LINE},
        InstallLocationTestCase{ManifestLocation::kExternalPolicy,
                                ExtensionInfo::EXTERNAL_POLICY},
        InstallLocationTestCase{ManifestLocation::kExternalComponent,
                                ExtensionInfo::EXTERNAL_COMPONENT}));

}  // namespace enterprise_connectors
