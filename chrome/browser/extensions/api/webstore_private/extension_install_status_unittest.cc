// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/extension_install_status.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_set.h"

using extensions::mojom::APIPermissionID;

namespace extensions {
namespace {
constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionSettingsWithUpdateUrlBlocking[] = R"({
  "update_url:https://clients2.google.com/service/update2/crx": {
    "installation_mode": "blocked"
  }
})";

constexpr char kExtensionSettingsWithWildcardBlocking[] = R"({
  "*": {
    "installation_mode": "blocked"
  }
})";

constexpr char kExtensionSettingsWithIdBlocked[] = R"({
  "abcdefghijklmnopabcdefghijklmnop": {
    "installation_mode": "blocked"
  }
})";

}  // namespace

class ExtensionInstallStatusTest : public BrowserWithTestWindowTest {
 public:
  ExtensionInstallStatusTest() = default;

  ExtensionInstallStatusTest(const ExtensionInstallStatusTest&) = delete;
  ExtensionInstallStatusTest& operator=(const ExtensionInstallStatusTest&) =
      delete;

  std::string GenerateArgs(const char* id) {
    return base::StringPrintf(R"(["%s"])", id);
  }

  scoped_refptr<const Extension> CreateExtension(const ExtensionId& id) {
    return ExtensionBuilder("extension").SetID(id).Build();
  }

  void SetExtensionSettings(const std::string& settings_string) {
    std::optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings);
    SetPolicy(pref_names::kExtensionManagement,
              base::Value::ToUniquePtrValue(std::move(*settings)));
  }

  void SetPolicy(const std::string& pref_name,
                 std::unique_ptr<base::Value> value) {
    profile()->GetTestingPrefService()->SetManagedPref(pref_name,
                                                       std::move(value));
  }

  void SetPendingList(const std::vector<ExtensionId>& ids) {
    base::Value::Dict id_values;
    for (const auto& id : ids) {
      base::Value::Dict request_data;
      request_data.Set(extension_misc::kExtensionRequestTimestamp,
                       ::base::TimeToValue(base::Time::Now()));
      id_values.Set(id, std::move(request_data));
    }
    profile()->GetTestingPrefService()->SetDict(
        prefs::kCloudExtensionRequestIds, std::move(id_values));
  }
};

TEST_F(ExtensionInstallStatusTest, ExtensionEnabled) {
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kEnabled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionDisabled) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kDisabled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionInstalledButDisabledByPolicy) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionTerminated) {
  ExtensionRegistry::Get(profile())->AddTerminated(
      CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kTerminated,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlocklisted) {
  ExtensionRegistry::Get(profile())->AddBlocklisted(
      CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kBlocklisted,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionAllowed) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionForceInstalledByPolicy) {
  SetExtensionSettings(R"({
    "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "force_installed",
      "update_url":"https://clients2.google.com/service/update2/crx"
    }
  })");
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kForceInstalled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedByUpdateUrl) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetExtensionSettings(kExtensionSettingsWithUpdateUrlBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedByWildcard) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetExtensionSettings(kExtensionSettingsWithWildcardBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedById) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest,
       ExtensionBlockByUpdateUrlWithRequestEnabled) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  SetExtensionSettings(kExtensionSettingsWithUpdateUrlBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockByWildcardWithRequestEnabled) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  SetExtensionSettings(kExtensionSettingsWithWildcardBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockByIdWithRequestEnabled) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  // An extension that is blocked by its ID can't be requested anymore.
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, PendingExtenisonIsWaitingToBeReviewed) {
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  std::vector<ExtensionId> ids = {kExtensionId};
  SetPendingList(ids);

  // The extension is blocked by wildcard and pending approval.
  SetExtensionSettings(kExtensionSettingsWithWildcardBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kRequestPending,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, PendingExtenisonIsApproved) {
  // Extension is approved but not installed, returns as INSTALLABLE.
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  std::vector<ExtensionId> ids = {kExtensionId};
  SetExtensionSettings(R"({
    "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "allowed"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, PendingExtenisonIsRejected) {
  // Extension is rejected, it should be moved from the pending list soon.
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  std::vector<ExtensionId> ids = {kExtensionId};
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

// If an existing, installed extension is disabled due to reason
// DISABLE_CUSTODIAN_APPROVAL_REQUIRED, then GetWebstoreExtensionInstallStatus()
// should return kCustodianApprovalRequired.
TEST_F(ExtensionInstallStatusTest,
       ExistingExtensionWithCustodianApprovalRequired) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  ExtensionPrefs::Get(profile())->AddDisableReason(
      kExtensionId,
      extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
  ASSERT_TRUE(
      ExtensionRegistry::Get(profile())->GetInstalledExtension(kExtensionId));

  EXPECT_EQ(ExtensionInstallStatus::kCustodianApprovalRequired,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedByManifestType) {
  // TYPE_EXTENSION is blocked by policy
  // TYPE_THEME and TYPE_HOSTED_APP are allowed.
  SetExtensionSettings(R"({
    "*": {
      "allowed_types": ["theme", "hosted_app"]
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_EXTENSION,
                                              PermissionSet()));
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_THEME,
                                              PermissionSet()));

  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_EXTENSION,
                                              PermissionSet()));
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_HOSTED_APP,
                                              PermissionSet()));

  // Request has been approved. Note that currently, manifest type blocking
  // actually overrides per-id setup. We will find the right priority with
  // crbug.com/1088016.
  SetExtensionSettings(R"({
    "*": {
      "allowed_types": ["theme", "hosted_app"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "allowed"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_EXTENSION,
                                              PermissionSet()));
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_HOSTED_APP,
                                              PermissionSet()));

  // Request has been rejected.
  SetExtensionSettings(R"({
    "*": {
      "allowed_types": ["theme", "hosted_app"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "blocked"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_EXTENSION,
                                              PermissionSet()));
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_HOSTED_APP,
                                              PermissionSet()));

  // Request has been forced installed.
  SetExtensionSettings(R"({
    "*": {
      "allowed_types": ["theme", "hosted_app"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "force_installed",
      "update_url":"https://clients2.google.com/service/update2/crx"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kForceInstalled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_EXTENSION,
                                              PermissionSet()));
  EXPECT_EQ(ExtensionInstallStatus::kForceInstalled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile(),
                                              Manifest::Type::TYPE_HOSTED_APP,
                                              PermissionSet()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionWithoutPermissionInfo) {
  SetExtensionSettings(R"({
    "*": {
      "blocked_permissions": ["storage"]
    }
  })");

  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionWithoutManifestInfo) {
  SetExtensionSettings(R"({
    "*": {
      "allowed_types": ["theme"]
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedByPermissions) {
  // Block 'storage' for all extensions.
  SetExtensionSettings(R"({
    "*": {
      "blocked_permissions": ["storage"]
    }
  })");

  // Extension with audio permission is still installable but not with storage.
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermissionID::kAudio);
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));
  api_permissions.insert(APIPermissionID::kStorage);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // And they can be requested,
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // Request has been approved.
  SetExtensionSettings(R"({
    "*": {
      "blocked_permissions": ["storage"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "allowed"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // Request has been rejected.
  SetExtensionSettings(R"({
    "*": {
      "blocked_permissions": ["storage"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "blocked"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // Request has been force installed.
  SetExtensionSettings(R"({
    "*": {
      "blocked_permissions": ["storage"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "force_installed",
      "update_url":"https://clients2.google.com/service/update2/crx"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kForceInstalled,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedByPermissionsWithUpdateUrl) {
  // Block 'downloads' for all extensions from web store.
  SetExtensionSettings(R"({
    "update_url:https://clients2.google.com/service/update2/crx": {
      "blocked_permissions": ["downloads"]
    }
  })");

  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermissionID::kAudio);
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));
  api_permissions.insert(APIPermissionID::kDownloads);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // And they can be requested,
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // Request has been approved.
  SetExtensionSettings(R"({
    "update_url:https://clients2.google.com/service/update2/crx": {
      "blocked_permissions": ["downloads"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "allowed"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // Request has been rejected.
  SetExtensionSettings(R"({
    "update_url:https://clients2.google.com/service/update2/crx": {
      "blocked_permissions": ["downloads"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "blocked"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));

  // Request has been force-installed.
  SetExtensionSettings(R"({
    "update_url:https://clients2.google.com/service/update2/crx": {
      "blocked_permissions": ["downloads"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "force_installed",
      "update_url":"https://clients2.google.com/service/update2/crx"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kForceInstalled,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));
}

TEST_F(ExtensionInstallStatusTest,
       ExtensionBlockedByPermissionButAllowlistById) {
  SetExtensionSettings(R"({
    "*": {
      "blocked_permissions": ["storage"]
    }, "abcdefghijklmnopabcdefghijklmnop": {
      "installation_mode": "allowed"
  }})");

  // Per-id allowlisted has higher priority than blocked permissions.
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermissionID::kStorage);
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));
}

// Extension policies apply to non web store update url doesn't affect the
// status here.
TEST_F(ExtensionInstallStatusTest, NonWebstoreUpdateUrlPolicy) {
  SetExtensionSettings(R"({
    "update_url:https://other.extensions/webstore": {
      "installation_mode": "blocked"
    }
  })");
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));

  SetExtensionSettings(R"({
    "update_url:https://other.extensions/webstore": {
      "blocked_permissions": ["downloads"]
    }
  })");
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermissionID::kDownloads);
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet())));
}

TEST_F(ExtensionInstallStatusTest, ManifestVersionIsBlocked) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/3));
  SetPolicy(pref_names::kManifestV2Availability,
            std::make_unique<base::Value>(static_cast<int>(
                internal::GlobalSettings::ManifestV2Setting::kDisabled)));
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/3));
}

TEST_F(ExtensionInstallStatusTest,
       ManifestVersionIsBlockedWithExtensionRequest) {
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/3));
  SetPolicy(pref_names::kManifestV2Availability,
            std::make_unique<base::Value>(static_cast<int>(
                internal::GlobalSettings::ManifestV2Setting::kDisabled)));
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/3));
}

// If an existing, installed extension is disabled due to corruption, then
// GetWebstoreExtensionInstallStatus() should return kCorrupted.
TEST_F(ExtensionInstallStatusTest, ExtensionCorrupted) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  ExtensionPrefs::Get(profile())->AddDisableReason(
      kExtensionId, extensions::disable_reason::DISABLE_CORRUPTED);
  EXPECT_EQ(ExtensionInstallStatus::kCorrupted,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

class SupervisedUserExtensionInstallStatusTest
    : public ExtensionInstallStatusTest {
 public:
  SupervisedUserExtensionInstallStatusTest() {
    std::vector<base::test::FeatureRef> enabled_features;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    enabled_features.push_back(
        supervised_user::
            kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    enabled_features.push_back(
        supervised_user::kExposedParentalControlNeededForExtensionInstallation);
    feature_list_.InitWithFeatures(enabled_features, /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If a supervised user requires parent approval to install a new extension that
// has not received parental approval before, then
// GetWebstoreExtensionInstallStatus() should return
// kCustodianApprovalRequiredForInstallation.
TEST_F(SupervisedUserExtensionInstallStatusTest,
       NewExtensionWithCustodianApprovalRequiredForInstallation) {
  profile()->SetIsSupervisedProfile(true);
  // The supervised user requires parent approval to install extensions.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  EXPECT_EQ(ExtensionInstallStatus::kCustodianApprovalRequiredForInstallation,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

// If a supervised user can skip parent permission to install extensions,
// then for a new (uninstalled) extension GetWebstoreExtensionInstallStatus()
// should return kInstallable.
TEST_F(
    SupervisedUserExtensionInstallStatusTest,
    NewExtensionOnSkipApprovalModeDoesNotRequireCustodianApprovalForInstallation) {
  profile()->SetIsSupervisedProfile(true);
  // The supervised user does not require parent approval to install extensions.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

// If a supervised user wants to install an extension that has been already
// granted parent approval (e.g. via synced settings from another client),
// then GetWebstoreExtensionInstallStatus() should return kInstallable.
TEST_F(
    SupervisedUserExtensionInstallStatusTest,
    NewExtensionWithParentApprovalDoesNotRequireCustodianApprovalForInstallation) {
  profile()->SetIsSupervisedProfile(true);
  // The supervised user requires parent approval to install extensions.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  // Grant approval to the extension.
  base::Value::Dict approved_extensions;
  approved_extensions.Set(kExtensionId, true);
  profile()->GetPrefs()->SetDict(prefs::kSupervisedUserApprovedExtensions,
                                 std::move(approved_extensions));

  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

// A test suite to toggle the behavior of the MV2 deprecation experiment.
class ExtensionInstallStatusTestWithMV2Deprecation
    : public ExtensionInstallStatusTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionInstallStatusTestWithMV2Deprecation() {
    feature_list_.InitWithFeatureState(
        extensions_features::kExtensionManifestV2Disabled, GetParam());
  }
  ~ExtensionInstallStatusTestWithMV2Deprecation() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

using ExtensionInstallStatusTestWithMV2DeprecationEnabled =
    ExtensionInstallStatusTestWithMV2Deprecation;

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionInstallStatusTestWithMV2Deprecation,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionInstallStatusTestWithMV2DeprecationEnabled,
                         testing::Values(true));

// Tests the webstore properly checks whether an extension can be installed
// inline with the MV2 Deprecation experiments.
TEST_P(ExtensionInstallStatusTestWithMV2Deprecation,
       MV2ExtensionsAreBlockedWithExperiment) {
  const ExtensionId kTestId(32, 'a');

  // MV3 extensions are always installable.
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(
                kTestId, profile(), Manifest::TYPE_EXTENSION, PermissionSet(),
                /*manifest_version=*/3));

  // MV2 extensions should be unavailable if and only if the experiment is
  // enabled.
  ExtensionInstallStatus expected_status =
      GetParam() ? ExtensionInstallStatus::kDeprecatedManifestVersion
                 : ExtensionInstallStatus::kInstallable;
  EXPECT_EQ(expected_status,
            GetWebstoreExtensionInstallStatus(
                kTestId, profile(), Manifest::TYPE_EXTENSION, PermissionSet(),
                /*manifest_version=*/2));
}

// An extension explicitly blocked by the admin should be considered blocked
// by policy, rather than a deprecated manifest version.
TEST_P(ExtensionInstallStatusTestWithMV2DeprecationEnabled,
       IdBlockedByPolicyTakesPriorityOverDeprecatedManifestVersion) {
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
}

// If an admin blocks all MV2 extensions, they should be considered blocked by
// policy, rather than a deprecated manifest version.
TEST_P(ExtensionInstallStatusTestWithMV2DeprecationEnabled,
       ManifestV2PolicyTakesPriorityOverDeprecatedManifestVersion) {
  SetPolicy(pref_names::kManifestV2Availability,
            std::make_unique<base::Value>(static_cast<int>(
                internal::GlobalSettings::ManifestV2Setting::kDisabled)));
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
}

// Extensions that are installed and enabled should indicate such, even if they
// are using a deprecated manifest version (since they are either re-enabled by
// the user or are allowed by the admin).
TEST_P(ExtensionInstallStatusTestWithMV2DeprecationEnabled,
       EnabledTakesPriorityOverDeprecatedManifestVersion) {
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kEnabled,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
}

// Extensions that are installed and disabled and have a deprecated manifest
// version should indicate they are unsupported due to the manifest version.
// Note, this applies even if they are disabled due to other reasons.
TEST_P(ExtensionInstallStatusTestWithMV2DeprecationEnabled,
       DeprecatedManifestVersionTakesPriorityOverDisabled) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kDeprecatedManifestVersion,
            GetWebstoreExtensionInstallStatus(
                kExtensionId, profile(), Manifest::Type::TYPE_EXTENSION,
                PermissionSet(), /*manifest_version=*/2));
}

}  // namespace extensions
