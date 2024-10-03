// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::mojom::APIPermissionID;
using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

const char kTargetExtension[] = "abcdefghijklmnopabcdefghijklmnop";
const char kTargetExtension2[] = "bcdefghijklmnopabcdefghijklmnopa";
const char kTargetExtension3[] = "cdefghijklmnopabcdefghijklmnopab";
const char kTargetExtension4[] = "defghijklmnopabcdefghijklmnopabc";
const char kTargetExtension5[] = "efghijklmnopabcdefghijklmnopabcd";
const char kTargetExtension6[] = "fghijklmnopabcdefghijklmnopabcde";
const char kTargetExtension7[] = "ghijklmnopabcdefghijklmnopabcdef";
const char kTargetExtension8[] = "hijklmnopabcdefghijklmnopabcdefg";
const char kTargetExtension9[] = "ijklmnopabcdefghijklmnopabcdefgh";
const char kExampleUpdateUrl[] = "http://example.com/update_url";

const char kNonExistingExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kNonExistingUpdateUrl[] = "http://example.net/update.xml";

const char kExampleForceInstalledDictPreference[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "installation_mode" : "force_installed",
    "update_url" : "http://example.com/update_url",
    "override_update_url": true,
  },
  "bcdefghijklmnopabcdefghijklmnopa" : {
    "installation_mode" : "force_installed",
    "update_url" : "http://example.com/update_url"
  }
})";

const char kExampleDictPreferenceWithoutInstallationMode[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "override_update_url": true,
  },
  "bcdefghijklmnopabcdefghijklmnopa" : {
    "minimum_version_required": "1.1.0"
  }
})";

const char kExampleDictPreferenceWithMultipleEntries[] = R"({
  "abcdefghijklmnopabcdefghijklmnop,bcdefghijklmnopabcdefghijklmnopa" : {
    "installation_mode": "blocked",
  },
  "bcdefghijklmnopabcdefghijklmnopa,cdefghijklmnopabcdefghijklmnopab" : {
    "minimum_version_required": "2.0"
  }
})";

const char kExampleDictPreference[] =
    R"(
{
  "abcdefghijklmnopabcdefghijklmnop": {
    "installation_mode": "allowed",
    "blocked_permissions": ["fileSystem", "bookmarks", "downloads"],
    "minimum_version_required": "1.1.0",
    "runtime_allowed_hosts": ["<all_urls>"],
  },
  "bcdefghijklmnopabcdefghijklmnopa": {
    "installation_mode": "force_installed",
    "update_url": "http://example.com/update_url",
    "blocked_permissions": ["downloads"],
  },
  "cdefghijklmnopabcdefghijklmnopab": {
    "installation_mode": "normal_installed",
    "update_url": "http://example.com/update_url",
    "blocked_permissions": ["fileSystem", "history"],
  },
  "defghijklmnopabcdefghijklmnopabc": {
    "installation_mode": "blocked",
    "runtime_blocked_hosts": ["*://*.foo.com", "https://bar.org/test"],
    "blocked_install_message": "Custom Error Extension4",
  },
  "efghijklmnopabcdefghijklmnopabcd,fghijklmnopabcdefghijklmnopabcde": {
    "installation_mode": "allowed",
  },
  "ghijklmnopabcdefghijklmnopabcdef,hijklmnopabcdefghijklmnopabcdefg,": {
    "installation_mode": "allowed",
  },
  "ijklmnopabcdefghijklmnopabcdefgh": {
    "installation_mode": "removed",
  },
  "update_url:http://example.com/update_url": {
    "installation_mode": "allowed",
    "blocked_permissions": ["fileSystem", "bookmarks"],
  },
  "*": {
    "installation_mode": "blocked",
    "install_sources": ["*://foo.com/*"],
    "allowed_types": ["theme", "user_script"],
    "blocked_permissions": ["fileSystem", "downloads"],
    "runtime_blocked_hosts": ["*://*.example.com"],
    "blocked_install_message": "Custom Error Default",
  },
})";

const char kExampleDictNoCustomError[] =
    "{"
    "  \"*\": {"
    "    \"installation_mode\": \"blocked\","
    "  },"
    "}";

}  // namespace

class ExtensionManagementServiceTest : public testing::Test {
 public:
  typedef ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>
      PrefUpdater;

  ExtensionManagementServiceTest() = default;
  ~ExtensionManagementServiceTest() override = default;

  // testing::Test:
  void SetUp() override { InitPrefService(); }

  void InitPrefService() {
    extension_management_.reset();
    profile_ = std::make_unique<TestingProfile>();
    pref_service_ = profile_->GetTestingPrefService();
    extension_management_ =
        std::make_unique<ExtensionManagement>(profile_.get());
  }

  void SetPref(bool managed,
               const char* path,
               std::unique_ptr<base::Value> value) {
    if (managed)
      pref_service_->SetManagedPref(path, std::move(value));
    else
      pref_service_->SetUserPref(path, std::move(value));
  }

  void SetPref(bool managed, const char* path, base::Value value) {
    SetPref(managed, path, base::Value::ToUniquePtrValue(std::move(value)));
  }

  void SetPref(bool managed, const char* path, base::Value::Dict dict) {
    SetPref(managed, path, base::Value(std::move(dict)));
  }

  void RemovePref(bool managed, const char* path) {
    if (managed)
      pref_service_->RemoveManagedPref(path);
    else
      pref_service_->RemoveUserPref(path);
  }

  const internal::GlobalSettings* ReadGlobalSettings() {
    return extension_management_->global_settings_.get();
  }

  ExtensionManagement::InstallationMode GetInstallationModeById(
      const std::string& id) {
    return GetInstallationMode(id, kNonExistingUpdateUrl);
  }

  ExtensionManagement::InstallationMode GetInstallationModeByUpdateUrl(
      const std::string& update_url) {
    return GetInstallationMode(kNonExistingExtension, update_url);
  }

  void CheckAutomaticallyInstalledUpdateUrl(const std::string& id,
                                            const std::string& update_url) {
    auto iter = extension_management_->settings_by_id_.find(id);
    ASSERT_TRUE(iter != extension_management_->settings_by_id_.end());
    ASSERT_TRUE((iter->second->installation_mode ==
                 ExtensionManagement::INSTALLATION_FORCED) ||
                (iter->second->installation_mode ==
                 ExtensionManagement::INSTALLATION_RECOMMENDED));
    EXPECT_EQ(iter->second->update_url, update_url);
  }

  APIPermissionSet GetBlockedAPIPermissionsById(const std::string& id) {
    return GetBlockedAPIPermissions(id, kNonExistingUpdateUrl);
  }

  APIPermissionSet GetBlockedAPIPermissionsByUpdateUrl(
      const std::string& update_url) {
    return GetBlockedAPIPermissions(kNonExistingExtension, update_url);
  }

  void SetExampleDictPref(std::string_view example_dict_preference) {
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        example_dict_preference,
        base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_TRUE(result->is_dict());
    SetPref(true, pref_names::kExtensionManagement, std::move(*result));
  }

  // Wrapper of ExtensionManagement::GetInstallationMode, |id| and
  // |update_url| are used to construct an Extension for testing.
  ExtensionManagement::InstallationMode GetInstallationMode(
      const std::string& id,
      const std::string& update_url) {
    scoped_refptr<const Extension> extension =
        CreateExtension(ManifestLocation::kUnpacked, "0.1", id, update_url);
    return extension_management_->GetInstallationMode(extension.get());
  }

  // Wrapper of ExtensionManagement::GetPolicyBlockedHosts, |id| is used
  // to construct an Extension for testing.
  URLPatternSet GetPolicyBlockedHosts(const std::string& id) {
    scoped_refptr<const Extension> extension = CreateExtension(
        ManifestLocation::kUnpacked, "0.1", id, kNonExistingUpdateUrl);
    return extension_management_->GetPolicyBlockedHosts(extension.get())
        .Clone();
  }

  // Wrapper of ExtensionManagement::GetPolicyAllowedHosts, |id| is used
  // to construct an Extension for testing.
  URLPatternSet GetPolicyAllowedHosts(const std::string& id) {
    scoped_refptr<const Extension> extension = CreateExtension(
        ManifestLocation::kUnpacked, "0.1", id, kNonExistingUpdateUrl);
    return extension_management_->GetPolicyAllowedHosts(extension.get())
        .Clone();
  }

  // Wrapper of ExtensionManagement::BlockedInstallMessage, |id| is used
  // in case the message is extension specific.
  const std::string GetBlockedInstallMessage(const std::string& id) {
    return extension_management_->BlockedInstallMessage(id);
  }

  // Wrapper of ExtensionManagement::GetBlockedAPIPermissions, |id| and
  // |update_url| are used to construct an Extension for testing.
  APIPermissionSet GetBlockedAPIPermissions(const std::string& id,
                                            const std::string& update_url) {
    scoped_refptr<const Extension> extension =
        CreateExtension(ManifestLocation::kUnpacked, "0.1", id, update_url);
    return extension_management_->GetBlockedAPIPermissions(extension.get());
  }

  // Wrapper of ExtensionManagement::CheckMinimumVersion, |id| and
  // |version| are used to construct an Extension for testing.
  bool CheckMinimumVersion(const std::string& id, const std::string& version) {
    scoped_refptr<const Extension> extension = CreateExtension(
        ManifestLocation::kUnpacked, version, id, kNonExistingUpdateUrl);
    std::string minimum_version_required;
    bool ret = extension_management_->CheckMinimumVersion(
        extension.get(), &minimum_version_required);
    EXPECT_EQ(ret, minimum_version_required.empty());
    EXPECT_EQ(ret, extension_management_->CheckMinimumVersion(extension.get(),
                                                              nullptr));
    return ret;
  }

 protected:
  scoped_refptr<const Extension> CreateExtensionHelper(
      ManifestLocation location,
      const std::string& version,
      const std::string& id,
      const std::string& update_url,
      int flags) {
    base::Value::Dict manifest_dict;
    manifest_dict.Set(manifest_keys::kName, "test");
    manifest_dict.Set(manifest_keys::kVersion, version);
    manifest_dict.Set(manifest_keys::kManifestVersion, 2);
    manifest_dict.Set(manifest_keys::kUpdateURL, update_url);
    std::string error;
    scoped_refptr<const Extension> extension =
        Extension::Create(base::FilePath(), location, std::move(manifest_dict),
                          flags, id, &error);
    CHECK(extension.get()) << error;
    return extension;
  }

  // Create an extension with specified |location|, |version|, |id| and
  // |update_url|. The extension created is NOT marked as originating from CWS.
  scoped_refptr<const Extension> CreateExtension(
      ManifestLocation location,
      const std::string& version,
      const std::string& id,
      const std::string& update_url) {
    return CreateExtensionHelper(location, version, id, update_url,
                                 Extension::NO_FLAGS);
  }

  scoped_refptr<const Extension> CreateOffstoreExtension(
      const std::string& id) {
    return CreateExtensionHelper(ManifestLocation::kUnpacked, "0.1", id,
                                 kNonExistingUpdateUrl, Extension::NO_FLAGS);
  }
  scoped_refptr<const Extension> CreateNormalExtension(const std::string& id) {
    return CreateExtensionHelper(ManifestLocation::kInternal, "0.1", id,
                                 extension_urls::kChromeWebstoreUpdateURL,
                                 Extension::FROM_WEBSTORE);
  }

  scoped_refptr<const Extension> CreateForcedExtension(const std::string& id) {
    return CreateForcedExtension(id, Extension::FROM_WEBSTORE);
  }

  scoped_refptr<const Extension> CreateForcedExtension(const std::string& id,
                                                       int flags) {
    scoped_refptr<const Extension> extension = CreateExtensionHelper(
        ManifestLocation::kExternalPolicy, "0.1", id, kExampleUpdateUrl, flags);
    base::Value::Dict forced_list_pref;
    ExternalPolicyLoader::AddExtension(forced_list_pref, id, kExampleUpdateUrl);
    SetPref(true, pref_names::kInstallForceList, forced_list_pref.Clone());
    return extension;
  }

  void SetExtensionLastUpdateTime(const std::string& id,
                                  base::Time update_time) {
    auto* extension_prefs = ExtensionPrefs::Get(profile_.get());
    extension_prefs->SetTimePref(
        id,
        {"last_update_time", PrefType::kTime, PrefScope::kExtensionSpecific},
        update_time);
  }

  bool IsUpdateUrlOverridden(const ExtensionId& extension_id) {
    return extension_management_->IsUpdateUrlOverridden(extension_id);
  }

  void SetCWSInfoService(CWSInfoServiceInterface* cws_info_service) {
    extension_management_->cws_info_service_ = cws_info_service;
  }

  bool IsFileUrlNavigationAllowed(const ExtensionId& extension_id) {
    return extension_management_->IsFileUrlNavigationAllowed(extension_id);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<ExtensionManagement> extension_management_;
};

class MockCWSInfoService : public CWSInfoServiceInterface {
 public:
  MOCK_METHOD(std::optional<bool>,
              IsLiveInCWS,
              (const Extension&),
              (const, override));
  MOCK_METHOD(std::optional<CWSInfoServiceInterface::CWSInfo>,
              GetCWSInfo,
              (const Extension&),
              (const, override));
  MOCK_METHOD(void, CheckAndMaybeFetchInfo, (), (override));
  MOCK_METHOD(void,
              AddObserver,
              (CWSInfoServiceInterface::Observer*),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (CWSInfoServiceInterface::Observer*),
              (override));
};

class ExtensionAdminPolicyTest : public ExtensionManagementServiceTest {
 public:
  ExtensionAdminPolicyTest() {}
  ~ExtensionAdminPolicyTest() override {}

  void SetUpPolicyProvider() {
    provider_ = std::make_unique<StandardManagementPolicyProvider>(
        extension_management_.get(), profile_.get());
  }

  void CreateExtension(ManifestLocation location) {
    base::Value::Dict values;
    CreateExtensionFromValues(location, &values);
  }

  void CreateHostedApp(ManifestLocation location) {
    base::Value::Dict values;
    values.SetByDottedPath(manifest_keys::kWebURLs,
                           base::Value(base::Value::Type::LIST));
    values.SetByDottedPath(manifest_keys::kLaunchWebURL,
                           "http://www.example.com");
    CreateExtensionFromValues(location, &values);
  }

  void CreateExtensionFromValues(ManifestLocation location,
                                 base::Value::Dict* values) {
    values->Set(manifest_keys::kName, "test");
    values->Set(manifest_keys::kVersion, "0.1");
    values->Set(manifest_keys::kManifestVersion, 2);
    std::string error;
    extension_ = Extension::Create(base::FilePath(), location, *values,
                                   Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension_.get());
  }

  // Wrappers for legacy admin policy functions, for testing purpose only.
  bool BlocklistedByDefault(const base::Value::List* blocklist);
  bool UserMayLoad(const base::Value::List* blocklist,
                   const base::Value::List* allowlist,
                   const base::Value::List* allowed_types,
                   const Extension* extension,
                   std::u16string* error);
  bool UserMayModifySettings(const Extension* extension, std::u16string* error);
  bool ExtensionMayModifySettings(const Extension* source_extension,
                                  const Extension* extension,
                                  std::u16string* error);
  bool MustRemainEnabled(const Extension* extension, std::u16string* error);

 protected:
  std::unique_ptr<StandardManagementPolicyProvider> provider_;
  scoped_refptr<Extension> extension_;
};

bool ExtensionAdminPolicyTest::BlocklistedByDefault(
    const base::Value::List* blocklist) {
  SetUpPolicyProvider();
  if (blocklist)
    SetPref(true, pref_names::kInstallDenyList,
            base::Value(blocklist->Clone()));
  return extension_management_->BlocklistedByDefault();
}

bool ExtensionAdminPolicyTest::UserMayLoad(
    const base::Value::List* blocklist,
    const base::Value::List* allowlist,
    const base::Value::List* allowed_types,
    const Extension* extension,
    std::u16string* error) {
  SetUpPolicyProvider();
  if (blocklist)
    SetPref(true, pref_names::kInstallDenyList,
            base::Value(blocklist->Clone()));
  if (allowlist)
    SetPref(true, pref_names::kInstallAllowList,
            base::Value(allowlist->Clone()));
  if (allowed_types)
    SetPref(true, pref_names::kAllowedTypes,
            base::Value(allowed_types->Clone()));
  return provider_->UserMayLoad(extension, error);
}

bool ExtensionAdminPolicyTest::UserMayModifySettings(const Extension* extension,
                                                     std::u16string* error) {
  SetUpPolicyProvider();
  return provider_->UserMayModifySettings(extension, error);
}

bool ExtensionAdminPolicyTest::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    std::u16string* error) {
  SetUpPolicyProvider();
  return provider_->ExtensionMayModifySettings(source_extension, extension,
                                               error);
}

bool ExtensionAdminPolicyTest::MustRemainEnabled(const Extension* extension,
                                                 std::u16string* error) {
  SetUpPolicyProvider();
  return provider_->MustRemainEnabled(extension, error);
}

// Verify that preference controlled by legacy ExtensionInstallSources policy is
// handled well.
TEST_F(ExtensionManagementServiceTest, LegacyInstallSources) {
  base::Value::List allowed_sites_pref;
  allowed_sites_pref.Append("https://www.example.com/foo");
  allowed_sites_pref.Append("https://corp.mycompany.com/*");
  SetPref(true, pref_names::kAllowedInstallSites,
          base::Value(std::move(allowed_sites_pref)));
  ASSERT_TRUE(ReadGlobalSettings()->install_sources);
  const URLPatternSet& allowed_sites = *ReadGlobalSettings()->install_sources;
  EXPECT_FALSE(allowed_sites.is_empty());
  EXPECT_TRUE(allowed_sites.MatchesURL(GURL("https://www.example.com/foo")));
  EXPECT_FALSE(allowed_sites.MatchesURL(GURL("https://www.example.com/bar")));
  EXPECT_TRUE(
      allowed_sites.MatchesURL(GURL("https://corp.mycompany.com/entry")));
  EXPECT_FALSE(
      allowed_sites.MatchesURL(GURL("https://www.mycompany.com/entry")));
}

// Verify that preference controlled by legacy ExtensionAllowedTypes policy is
// handled well.
TEST_F(ExtensionManagementServiceTest, LegacyAllowedTypes) {
  base::Value::List allowed_types_pref;
  allowed_types_pref.Append(Manifest::TYPE_THEME);
  allowed_types_pref.Append(Manifest::TYPE_USER_SCRIPT);

  SetPref(true, pref_names::kAllowedTypes,
          base::Value(std::move(allowed_types_pref)));
  ASSERT_TRUE(ReadGlobalSettings()->allowed_types);
  const std::vector<Manifest::Type>& allowed_types =
      *ReadGlobalSettings()->allowed_types;
  EXPECT_EQ(allowed_types.size(), 2u);
  EXPECT_FALSE(base::Contains(allowed_types, Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_THEME));
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_USER_SCRIPT));
}

// Verify that preference controlled by legacy ExtensionInstallBlocklist policy
// is handled well.
TEST_F(ExtensionManagementServiceTest, LegacyInstallBlocklist) {
  base::Value::List denied_list_pref;
  denied_list_pref.Append(kTargetExtension);

  SetPref(true, pref_names::kInstallDenyList,
          base::Value(std::move(denied_list_pref)));
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Verify that preference controlled by legacy ExtensionInstallAllowlist policy
// is handled well.
TEST_F(ExtensionManagementServiceTest, LegacyAllowlist) {
  base::Value::List denied_list_pref;
  denied_list_pref.Append("*");
  base::Value::List allowed_list_pref;
  allowed_list_pref.Append(kTargetExtension);

  SetPref(true, pref_names::kInstallDenyList,
          base::Value(std::move(denied_list_pref)));
  SetPref(true, pref_names::kInstallAllowList,
          base::Value(allowed_list_pref.Clone()));
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);

  // Verify that install allowlist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallAllowList);
  SetPref(false, pref_names::kInstallAllowList,
          base::Value(std::move(allowed_list_pref)));
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
}

// Verify that preference controlled by legacy ExtensionInstallForcelist policy
// is handled well.
TEST_F(ExtensionManagementServiceTest, LegacyInstallForcelist) {
  base::Value::Dict forced_list_pref;
  ExternalPolicyLoader::AddExtension(forced_list_pref, kTargetExtension,
                                     kExampleUpdateUrl);

  SetPref(true, pref_names::kInstallForceList, forced_list_pref.Clone());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  CheckAutomaticallyInstalledUpdateUrl(kTargetExtension, kExampleUpdateUrl);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);

  // Verify that install forcelist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallForceList);
  SetPref(false, pref_names::kInstallForceList, forced_list_pref.Clone());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Verify that update url is overridden for extensions specified in
// |kInstallForcelist| pref but |installation_mode| is missing in
// |kExtensionSettings| pref.
TEST_F(ExtensionManagementServiceTest,
       InstallUpdateUrlEnforcedForceInstalledPref) {
  base::Value::Dict forced_list_pref;
  ExternalPolicyLoader::AddExtension(forced_list_pref, kTargetExtension,
                                     kExampleUpdateUrl);
  ExternalPolicyLoader::AddExtension(forced_list_pref, kTargetExtension2,
                                     kExampleUpdateUrl);

  SetPref(true, pref_names::kInstallForceList, forced_list_pref.Clone());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_FORCED);

  SetExampleDictPref(kExampleDictPreferenceWithoutInstallationMode);

  // Verify that the update URL is overridden for kTargetExtension.
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_TRUE(IsUpdateUrlOverridden(kTargetExtension));

  // Verify that the update URL is not overridden for kTargetExtension2 because
  // |override_update_url| flag is not specified for it in |kExtensionSettings|
  // pref.
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_FALSE(IsUpdateUrlOverridden(kTargetExtension2));
}

// Verify that update url is not overridden for extensions not specified in
// |kInstallForcelist| and |installation_mode| is missing in
// |kExtensionSettings|.
TEST_F(ExtensionManagementServiceTest,
       InstallUpdateUrlEnforcedForceInstalledPrefMissing) {
  base::Value::Dict forced_list_pref;
  ExternalPolicyLoader::AddExtension(forced_list_pref, kTargetExtension2,
                                     kExampleUpdateUrl);
  SetPref(true, pref_names::kInstallForceList, forced_list_pref.Clone());

  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_FORCED);

  SetExampleDictPref(kExampleDictPreferenceWithoutInstallationMode);

  // Verify that the update URL is not overridden for kTargetExtension as it is
  // not listed in |kInstallForcelist| pref.
  EXPECT_NE(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_FALSE(IsUpdateUrlOverridden(kTargetExtension));
}

// Verify that update url is overridden for extensions which are marked as
// 'force_installed' and |override_update_url| is true for them in
// |kExtensionSettings|.
TEST_F(ExtensionManagementServiceTest,
       InstallUpdateUrlEnforcedExtensionSettings) {
  SetExampleDictPref(kExampleForceInstalledDictPreference);

  // Verify that the update URL is overridden for kTargetExtension.
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_TRUE(IsUpdateUrlOverridden(kTargetExtension));

  // Verify that the update URL is not overridden for kTargetExtension2 because
  // |override_update_url| flag is not specified for it in |kExtensionSettings|
  // pref.
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_FALSE(IsUpdateUrlOverridden(kTargetExtension2));
}

// Verify that the force-installed extension specified in the preference
// |kInstallUpdateUrlEnforced| is ignored if the update URL is a webstore update
// URL.
TEST_F(ExtensionManagementServiceTest,
       InstallUpdateUrlEnforcedWebstoreUpdateUrl) {
  base::Value::Dict forced_list_pref;
  ExternalPolicyLoader::AddExtension(forced_list_pref, kTargetExtension,
                                     extension_urls::kChromeWebstoreUpdateURL);
  ExternalPolicyLoader::AddExtension(forced_list_pref, kTargetExtension2,
                                     kExampleUpdateUrl);

  SetPref(true, pref_names::kInstallForceList, forced_list_pref.Clone());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_FORCED);

  SetExampleDictPref(kExampleDictPreferenceWithoutInstallationMode);

  // Verify that the update URL is not overridden for kTargetExtension because
  // |update_url| is a Chrome web store URL.
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_FALSE(IsUpdateUrlOverridden(kTargetExtension));
}

// Tests handling of exceeding number of urls
TEST_F(ExtensionManagementServiceTest, HostsMaximumExceeded) {
  const char policy_template[] =
      "{"
      "  \"abcdefghijklmnopabcdefghijklmnop\": {"
      "    \"installation_mode\": \"allowed\","
      "    \"runtime_blocked_hosts\": [%s],"
      "    \"runtime_allowed_hosts\": [%s]"
      "  }"
      "}";

  std::string urls;
  for (size_t i = 0; i < 200; ++i)
    urls.append("\"*://example" + base::NumberToString(i) + ".com\",");

  std::string policy =
      base::StringPrintf(policy_template, urls.c_str(), urls.c_str());
  SetExampleDictPref(policy);
  EXPECT_EQ(100u, GetPolicyBlockedHosts(kTargetExtension).size());
  EXPECT_EQ(100u, GetPolicyAllowedHosts(kTargetExtension).size());
}

// Tests that multiple entries for a dictionary are all applied.
TEST_F(ExtensionManagementServiceTest, MultipleEntries) {
  SetExampleDictPref(kExampleDictPreferenceWithMultipleEntries);

  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_BLOCKED);

  EXPECT_FALSE(CheckMinimumVersion(kTargetExtension2, "1.0"));
}

// Tests parsing of new dictionary preference.
TEST_F(ExtensionManagementServiceTest, PreferenceParsing) {
  SetExampleDictPref(kExampleDictPreference);

  // Verifies the installation mode settings.
  EXPECT_TRUE(extension_management_->BlocklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_FORCED);
  CheckAutomaticallyInstalledUpdateUrl(kTargetExtension2, kExampleUpdateUrl);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension3),
            ExtensionManagement::INSTALLATION_RECOMMENDED);
  CheckAutomaticallyInstalledUpdateUrl(kTargetExtension3, kExampleUpdateUrl);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeByUpdateUrl(kExampleUpdateUrl),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_TRUE(GetPolicyBlockedHosts(kTargetExtension).is_empty());
  EXPECT_TRUE(GetPolicyBlockedHosts(kTargetExtension4)
                  .MatchesURL(GURL("http://test.foo.com/test")));
  EXPECT_TRUE(GetPolicyBlockedHosts(kTargetExtension4)
                  .MatchesURL(GURL("https://bar.org/test")));
  EXPECT_TRUE(GetBlockedInstallMessage(kTargetExtension).empty());
  EXPECT_EQ("Custom Error Extension4",
            GetBlockedInstallMessage(kTargetExtension4));
  EXPECT_EQ("Custom Error Default",
            GetBlockedInstallMessage(kNonExistingExtension));

  // Verifies using multiple extensions as a key.
  EXPECT_EQ(GetInstallationModeById(kTargetExtension5),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension6),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension7),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension8),
            ExtensionManagement::INSTALLATION_ALLOWED);

  // Verifies global settings.
  ASSERT_TRUE(ReadGlobalSettings()->install_sources);
  const URLPatternSet& allowed_sites = *ReadGlobalSettings()->install_sources;
  EXPECT_EQ(allowed_sites.size(), 1u);
  EXPECT_TRUE(allowed_sites.MatchesURL(GURL("http://foo.com/entry")));
  EXPECT_FALSE(allowed_sites.MatchesURL(GURL("http://bar.com/entry")));
  EXPECT_TRUE(GetPolicyBlockedHosts(kNonExistingExtension)
                  .MatchesURL(GURL("http://example.com/default")));

  ASSERT_TRUE(ReadGlobalSettings()->allowed_types);
  const std::vector<Manifest::Type>& allowed_types =
      *ReadGlobalSettings()->allowed_types;
  EXPECT_EQ(allowed_types.size(), 2u);
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_THEME));
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_USER_SCRIPT));

  // Verifies blocked permission allowlist settings.
  APIPermissionSet api_permission_set;
  api_permission_set.clear();
  api_permission_set.insert(APIPermissionID::kFileSystem);
  api_permission_set.insert(APIPermissionID::kDownloads);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissionsById(kNonExistingExtension));

  api_permission_set.clear();
  api_permission_set.insert(APIPermissionID::kFileSystem);
  api_permission_set.insert(APIPermissionID::kDownloads);
  api_permission_set.insert(APIPermissionID::kBookmark);
  EXPECT_EQ(api_permission_set, GetBlockedAPIPermissionsById(kTargetExtension));

  api_permission_set.clear();
  api_permission_set.insert(APIPermissionID::kDownloads);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissionsById(kTargetExtension2));

  api_permission_set.clear();
  api_permission_set.insert(APIPermissionID::kFileSystem);
  api_permission_set.insert(APIPermissionID::kHistory);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissionsById(kTargetExtension3));

  api_permission_set.clear();
  api_permission_set.insert(APIPermissionID::kFileSystem);
  api_permission_set.insert(APIPermissionID::kBookmark);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissionsByUpdateUrl(kExampleUpdateUrl));

  // Verifies minimum version settings.
  EXPECT_FALSE(CheckMinimumVersion(kTargetExtension, "1.0.99"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "1.1"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "1.1.0.1"));

  // Verifies that an extension using the default scope where
  // no custom blocked install message is defined returns an empty string.
  SetExampleDictPref(kExampleDictNoCustomError);
  EXPECT_EQ("", GetBlockedInstallMessage(kNonExistingExtension));
}

// Tests the handling of installation mode in case it's specified in both
// per-extension and per-update-url settings.
TEST_F(ExtensionManagementServiceTest, InstallationModeConflictHandling) {
  SetExampleDictPref(kExampleDictPreference);

  // Per-extension installation mode settings should always override
  // per-update-url settings.
  EXPECT_EQ(GetInstallationMode(kTargetExtension, kExampleUpdateUrl),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationMode(kTargetExtension2, kExampleUpdateUrl),
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_EQ(GetInstallationMode(kTargetExtension3, kExampleUpdateUrl),
            ExtensionManagement::INSTALLATION_RECOMMENDED);
}

// Tests the handling of blocked permissions in case it's specified in both
// per-extension and per-update-url settings.
TEST_F(ExtensionManagementServiceTest, BlockedPermissionsConflictHandling) {
  SetExampleDictPref(kExampleDictPreference);

  // Both settings should be overridden.
  APIPermissionSet blocked_permissions_for_update_url;
  blocked_permissions_for_update_url.insert(APIPermissionID::kFileSystem);
  blocked_permissions_for_update_url.insert(APIPermissionID::kBookmark);

  APIPermissionSet api_permission_set;

  api_permission_set = blocked_permissions_for_update_url.Clone();
  api_permission_set.insert(APIPermissionID::kFileSystem);
  api_permission_set.insert(APIPermissionID::kDownloads);
  api_permission_set.insert(APIPermissionID::kBookmark);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissions(kTargetExtension, kExampleUpdateUrl));

  api_permission_set = blocked_permissions_for_update_url.Clone();
  api_permission_set.insert(APIPermissionID::kDownloads);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissions(kTargetExtension2, kExampleUpdateUrl));

  api_permission_set = blocked_permissions_for_update_url.Clone();
  api_permission_set.insert(APIPermissionID::kFileSystem);
  api_permission_set.insert(APIPermissionID::kHistory);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissions(kTargetExtension3, kExampleUpdateUrl));

  // Default blocked permissions will not be inherited.
  EXPECT_EQ(blocked_permissions_for_update_url,
            GetBlockedAPIPermissions(kTargetExtension4, kExampleUpdateUrl));
  EXPECT_EQ(
      APIPermissionSet(),
      GetBlockedAPIPermissions(kTargetExtension4,
                               "https://www.example.com/another_update_url"));
}

TEST_F(ExtensionManagementServiceTest, DefaultHostExtensionsOverride) {
  SetExampleDictPref(base::StringPrintf(
      R"({
    "%s": {
      "runtime_allowed_hosts": ["https://allow.extension.com"],
      "runtime_blocked_hosts": ["https://block.extension.com"],
    },
    "%s": {},
    "*": {
      "runtime_allowed_hosts": ["https://allow.default.com"],
      "runtime_blocked_hosts": ["https://block.default.com"],
    },
  })",
      kTargetExtension, kTargetExtension2));

  // Override allow/block host for the first extension.
  URLPatternSet expected_extension_allowed_set_1;
  URLPatternSet expected_extension_blocked_set_1;
  expected_extension_allowed_set_1.AddPattern(
      {URLPattern::SCHEME_ALL, "https://allow.extension.com/*"});
  expected_extension_blocked_set_1.AddPattern(
      {URLPattern::SCHEME_ALL, "https://block.extension.com/*"});

  EXPECT_EQ(expected_extension_allowed_set_1,
            GetPolicyAllowedHosts(kTargetExtension));
  EXPECT_EQ(expected_extension_blocked_set_1,
            GetPolicyBlockedHosts(kTargetExtension));

  // Empty allow/block host for the second extension.
  EXPECT_EQ(URLPatternSet(), GetPolicyAllowedHosts(kTargetExtension2));
  EXPECT_EQ(URLPatternSet(), GetPolicyBlockedHosts(kTargetExtension2));

  // Default allow/block host for the third extension.
  URLPatternSet expected_extension_allowed_set_3;
  URLPatternSet expected_extension_blocked_set_3;
  expected_extension_allowed_set_3.AddPattern(
      {URLPattern::SCHEME_ALL, "https://allow.default.com/*"});
  expected_extension_blocked_set_3.AddPattern(
      {URLPattern::SCHEME_ALL, "https://block.default.com/*"});

  EXPECT_EQ(expected_extension_allowed_set_3,
            GetPolicyAllowedHosts(kTargetExtension3));
  EXPECT_EQ(expected_extension_blocked_set_3,
            GetPolicyBlockedHosts(kTargetExtension3));
}

// Tests the 'minimum_version_required' settings of extension management.
TEST_F(ExtensionManagementServiceTest, kMinimumVersionRequired) {
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "0.0"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "3.0.0"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "9999.0"));

  {
    PrefUpdater pref(pref_service_.get());
    pref.SetMinimumVersionRequired(kTargetExtension, "3.0");
  }

  EXPECT_FALSE(CheckMinimumVersion(kTargetExtension, "0.0"));
  EXPECT_FALSE(CheckMinimumVersion(kTargetExtension, "2.99"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "3.0.0"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "3.0.1"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "4.0"));
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionInstallSources policy.
TEST_F(ExtensionManagementServiceTest, NewInstallSources) {
  // Set the legacy preference, and verifies that it works.
  base::Value::List allowed_sites_pref;
  allowed_sites_pref.Append("https://www.example.com/foo");
  SetPref(true, pref_names::kAllowedInstallSites,
          base::Value(std::move(allowed_sites_pref)));
  ASSERT_TRUE(ReadGlobalSettings()->install_sources);
  EXPECT_TRUE(ReadGlobalSettings()->install_sources->MatchesURL(
      GURL("https://www.example.com/foo")));

  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_.get());
    updater.ClearInstallSources();
  }
  // Verifies that the new one overrides the legacy ones.
  ASSERT_TRUE(ReadGlobalSettings()->install_sources);
  EXPECT_FALSE(ReadGlobalSettings()->install_sources->MatchesURL(
      GURL("https://www.example.com/foo")));

  // Updates the new dictionary preference.
  {
    PrefUpdater updater(pref_service_.get());
    updater.AddInstallSource("https://corp.mycompany.com/*");
  }
  ASSERT_TRUE(ReadGlobalSettings()->install_sources);
  EXPECT_TRUE(ReadGlobalSettings()->install_sources->MatchesURL(
      GURL("https://corp.mycompany.com/entry")));
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionAllowedTypes policy.
TEST_F(ExtensionManagementServiceTest, NewAllowedTypes) {
  // Set the legacy preference, and verifies that it works.
  base::Value::List allowed_types_pref;
  allowed_types_pref.Append(Manifest::TYPE_USER_SCRIPT);
  SetPref(true, pref_names::kAllowedTypes,
          base::Value(allowed_types_pref.Clone()));
  ASSERT_TRUE(ReadGlobalSettings()->allowed_types);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types->size(), 1u);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types.value()[0],
            Manifest::TYPE_USER_SCRIPT);

  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_.get());
    updater.ClearAllowedTypes();
  }
  // Verifies that the new one overrides the legacy ones.
  ASSERT_TRUE(ReadGlobalSettings()->allowed_types);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types->size(), 0u);

  // Updates the new dictionary preference.
  {
    PrefUpdater updater(pref_service_.get());
    updater.AddAllowedType("theme");
  }
  ASSERT_TRUE(ReadGlobalSettings()->allowed_types);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types->size(), 1u);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types.value()[0],
            Manifest::TYPE_THEME);
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionInstallBlocklist policy.
TEST_F(ExtensionManagementServiceTest, NewInstallBlocklist) {
  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_.get());
    updater.SetBlocklistedByDefault(false);  // Allowed by default.
    updater.SetIndividualExtensionInstallationAllowed(kTargetExtension, false);
    updater.ClearPerExtensionSettings(kTargetExtension2);
  }
  EXPECT_FALSE(extension_management_->BlocklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);

  // Set legacy preference.
  base::Value::List denied_list_pref;
  denied_list_pref.Append("*");
  denied_list_pref.Append(kTargetExtension2);
  SetPref(true, pref_names::kInstallDenyList,
          base::Value(std::move(denied_list_pref)));

  base::Value::List allowed_list_pref;
  allowed_list_pref.Append(kTargetExtension);
  SetPref(true, pref_names::kInstallAllowList,
          base::Value(std::move(allowed_list_pref)));

  // Verifies that the new one have higher priority over the legacy ones.
  EXPECT_FALSE(extension_management_->BlocklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionInstallAllowlist policy.
TEST_F(ExtensionManagementServiceTest, NewAllowlist) {
  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_.get());
    updater.SetBlocklistedByDefault(true);  // Disallowed by default.
    updater.SetIndividualExtensionInstallationAllowed(kTargetExtension, true);
    updater.ClearPerExtensionSettings(kTargetExtension2);
  }
  EXPECT_TRUE(extension_management_->BlocklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);

  // Set legacy preference.
  base::Value::List denied_list_pref;
  denied_list_pref.Append(kTargetExtension);
  SetPref(true, pref_names::kInstallDenyList,
          base::Value(std::move(denied_list_pref)));

  base::Value::List allowed_list_pref;
  allowed_list_pref.Append(kTargetExtension2);
  SetPref(true, pref_names::kInstallAllowList,
          base::Value(std::move(allowed_list_pref)));

  // Verifies that the new one have higher priority over the legacy ones.
  EXPECT_TRUE(extension_management_->BlocklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionInstallForcelist policy.
TEST_F(ExtensionManagementServiceTest, NewInstallForcelist) {
  // Set some legacy preferences, to verify that the new one overrides the
  // legacy ones.
  base::Value::List denied_list_pref;
  denied_list_pref.Append(kTargetExtension);
  SetPref(true, pref_names::kInstallDenyList,
          base::Value(std::move(denied_list_pref)));

  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_.get());
    updater.SetIndividualExtensionAutoInstalled(
        kTargetExtension, kExampleUpdateUrl, true);
  }
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  CheckAutomaticallyInstalledUpdateUrl(kTargetExtension, kExampleUpdateUrl);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Tests the behavior of IsInstallationExplicitlyAllowed().
TEST_F(ExtensionManagementServiceTest, IsInstallationExplicitlyAllowed) {
  SetExampleDictPref(kExampleDictPreference);

  // Constant name indicates the installation_mode of extensions in example
  // preference.
  const char* allowed = kTargetExtension;
  const char* forced  = kTargetExtension2;
  const char* recommended = kTargetExtension3;
  const char* blocked = kTargetExtension4;
  const char* removed = kTargetExtension9;
  const char* not_specified = kNonExistingExtension;

  // BlocklistedByDefault() is true in example preference.
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyAllowed(allowed));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyAllowed(forced));
  EXPECT_TRUE(
      extension_management_->IsInstallationExplicitlyAllowed(recommended));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyAllowed(blocked));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyAllowed(removed));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyAllowed(not_specified));

  // Set BlocklistedByDefault() to false.
  PrefUpdater pref(pref_service_.get());
  pref.SetBlocklistedByDefault(false);

  // The result should remain the same.
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyAllowed(allowed));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyAllowed(forced));
  EXPECT_TRUE(
      extension_management_->IsInstallationExplicitlyAllowed(recommended));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyAllowed(blocked));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyAllowed(removed));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyAllowed(not_specified));
}

TEST_F(ExtensionManagementServiceTest, IsInstallationExplicitlyBlocked) {
  SetExampleDictPref(kExampleDictPreference);

  // Constant name indicates the installation_mode of extensions in example
  // preference.
  const char* allowed = kTargetExtension;
  const char* forced = kTargetExtension2;
  const char* recommended = kTargetExtension3;
  const char* blocked = kTargetExtension4;
  const char* removed = kTargetExtension9;
  const char* not_specified = kNonExistingExtension;

  // BlocklistedByDefault() is true in example preference.
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(allowed));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(forced));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(recommended));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(blocked));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(removed));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(not_specified));

  PrefUpdater pref(pref_service_.get());
  pref.SetBlocklistedByDefault(false);

  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(allowed));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(forced));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(recommended));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(blocked));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(removed));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(not_specified));
}

TEST_F(ExtensionManagementServiceTest,
       ExtensionsAreBlockedByDefaultForExtensionRequest) {
  // When extension request policy is set to true, all extensions are blocked by
  // default.
  SetPref(true, prefs::kCloudExtensionRequestEnabled,
          std::make_unique<base::Value>(true));
  EXPECT_TRUE(extension_management_->BlocklistedByDefault());
  EXPECT_EQ(ExtensionManagement::INSTALLATION_BLOCKED,
            GetInstallationModeById(kTargetExtension));
  // However, it will be overridden by ExtensionSettings
  SetExampleDictPref(R"({
    "*": {
      "installation_mode": "removed",
    }
  })");
  EXPECT_EQ(ExtensionManagement::INSTALLATION_REMOVED,
            GetInstallationModeById(kTargetExtension));
}

TEST_F(ExtensionManagementServiceTest, ManifestV2Default) {
  SetPref(true, pref_names::kManifestV2Availability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::ManifestV2Setting::kDefault)));
  bool is_manifest_v3_only = base::FeatureList::IsEnabled(
      extensions_features::kExtensionsManifestV3Only);
  EXPECT_EQ(!is_manifest_v3_only,
            extension_management_->IsAllowedManifestVersion(
                2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));

  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  // Note: MV3 extension isn't exempt by policy because it's not affected at
  // all. It's not this class's responsibility to know about the rest of the
  // criteria; only whether the extension is exempt by policy.
  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
}

TEST_F(ExtensionManagementServiceTest, ManifestV2Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kExtensionsManifestV3Only);
  SetPref(true, pref_names::kManifestV2Availability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::ManifestV2Setting::kDisabled)));
  EXPECT_FALSE(extension_management_->IsAllowedManifestVersion(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));

  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  // Note: MV3 extension isn't exempt by policy because it's not affected at
  // all. It's not this class's responsibility to know about the rest of the
  // criteria; only whether the extension is exempt by policy.
  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
}

TEST_F(ExtensionManagementServiceTest, ManifestV2Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kExtensionsManifestV3Only);
  SetPref(true, pref_names::kManifestV2Availability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::ManifestV2Setting::kEnabled)));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));

  EXPECT_TRUE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  // Note: MV3 extension isn't exempt by policy because it's not affected at
  // all. It's not this class's responsibility to know about the rest of the
  // criteria; only whether the extension is exempt by policy.
  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
}

TEST_F(ExtensionManagementServiceTest, ManifestV2EnabledForForceInstalled) {
  SetPref(
      true, pref_names::kManifestV2Availability,
      base::Value(static_cast<int>(internal::GlobalSettings::ManifestV2Setting::
                                       kEnabledForForceInstalled)));
  EXPECT_FALSE(extension_management_->IsAllowedManifestVersion(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));

  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  // Note: MV3 extension isn't exempt by policy because it's not affected at
  // all. It's not this class's responsibility to know about the rest of the
  // criteria; only whether the extension is exempt by policy.
  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));

  base::Value::Dict forced_list_pref;
  ExternalPolicyLoader::AddExtension(forced_list_pref, kTargetExtension,
                                     kExampleUpdateUrl);
  SetPref(true, pref_names::kInstallForceList, forced_list_pref.Clone());

  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));

  EXPECT_TRUE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  // Note: MV3 extension isn't exempt by policy because it's not affected at
  // all. It's not this class's responsibility to know about the rest of the
  // criteria; only whether the extension is exempt by policy.
  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      3, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
}

TEST_F(ExtensionManagementServiceTest, ManifestV2EnabledForExtensionOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kExtensionsManifestV3Only);
  SetPref(true, pref_names::kManifestV2Availability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::ManifestV2Setting::kEnabled)));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  EXPECT_TRUE(extension_management_->IsAllowedManifestVersion(
      2, kTargetExtension, Manifest::Type::TYPE_LOGIN_SCREEN_EXTENSION));
  EXPECT_FALSE(extension_management_->IsAllowedManifestVersion(
      2, kTargetExtension, Manifest::Type::TYPE_HOSTED_APP));

  EXPECT_TRUE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_EXTENSION));
  EXPECT_TRUE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_LOGIN_SCREEN_EXTENSION));
  // Despite being force-installed, hosted apps aren't includede in the
  // MV2 deprecation, so isn't exempt by policy.
  EXPECT_FALSE(extension_management_->IsExemptFromMV2DeprecationByPolicy(
      2, kTargetExtension, Manifest::Type::TYPE_HOSTED_APP));
}

// Verifies that extensions that do not update CWS are always allowed by
// the ExtensionUnpublishedAvailability policy check function since this policy
// ignores them.
TEST_F(ExtensionManagementServiceTest,
       UnpublishedCheckForNonCWSUpdateExtensions) {
  // Create test extensions that don't update from CWS.
  scoped_refptr<const Extension> offstore_extension =
      CreateOffstoreExtension(kNonExistingExtension);
  scoped_refptr<const Extension> forced_extension =
      CreateForcedExtension(kTargetExtension3);
  // Create mock CWS service. Verify it is not queried for these policy
  // checks.
  testing::NiceMock<MockCWSInfoService> mock_cws_info_service;
  SetCWSInfoService(&mock_cws_info_service);
  EXPECT_CALL(mock_cws_info_service, GetCWSInfo).Times(0);
  // Verify that the extensions are allowed regardless of policy setting.
  SetPref(true, pref_names::kExtensionUnpublishedAvailability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::UnpublishedAvailability::
                  kAllowUnpublished)));
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      offstore_extension.get()));
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      forced_extension.get()));

  SetPref(true, pref_names::kExtensionUnpublishedAvailability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::UnpublishedAvailability::
                  kDisableUnpublished)));
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      offstore_extension.get()));
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      forced_extension.get()));
  SetCWSInfoService(nullptr);
}

// Verifies that a CWS extensions is allowed if the
// ExtensionUnpublishedAvailability policy setting is kAllowUnpublished.
TEST_F(ExtensionManagementServiceTest,
       UnpublishedCheckWithPolicySettingAllowUnpublished) {
  // Configure the policy.
  SetPref(true, pref_names::kExtensionUnpublishedAvailability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::UnpublishedAvailability::
                  kAllowUnpublished)));
  // Create a test extension.
  scoped_refptr<const Extension> normal_extension =
      CreateNormalExtension(kTargetExtension);
  // CWS publish state should not be queried when this extension is checked.
  testing::NiceMock<MockCWSInfoService> mock_cws_info_service;
  SetCWSInfoService(&mock_cws_info_service);
  EXPECT_CALL(mock_cws_info_service, GetCWSInfo).Times(0);
  // Verify that the extension is allowed.
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      normal_extension.get()));
  SetCWSInfoService(nullptr);
}

// If ExtensionUnpublishedAvailability policy setting is
// kDisableUnpublished, verify that:
// - an extension is allowed if it is currently published in CWS or if the
//   CWS publish information is missing
// - an extension is disallowed if it is not currently published in CWS
TEST_F(ExtensionManagementServiceTest,
       UnpublishedCheckWithPolicySettingDisableUnpublished) {
  // Configure the policy.
  SetPref(true, pref_names::kExtensionUnpublishedAvailability,
          base::Value(static_cast<int>(
              internal::GlobalSettings::UnpublishedAvailability::
                  kDisableUnpublished)));
  // Create a test extension.
  scoped_refptr<const Extension> normal_extension =
      CreateNormalExtension(kTargetExtension);
  // Create mock CWSInfoService to verify GetCWSInfo is called.
  testing::NiceMock<MockCWSInfoService> mock_cws_info_service;
  SetCWSInfoService(&mock_cws_info_service);
  // Set up responses to GetCWSInfo calls.
  CWSInfoServiceInterface::CWSInfo cws_info_live = {
      /*is_present=*/true,
      /*is_live=*/true,
      /*last_update_time=*/base::Time::Now(),
      CWSInfoServiceInterface::CWSViolationType::kNone,
      /*unpublished_long_ago=*/false,
      /*no_privacy_practice=*/false};
  CWSInfoServiceInterface::CWSInfo cws_info_not_live = {
      /*is_present=*/true,
      /*is_live=*/false,
      /*last_update_time=*/base::Time::Now(),
      CWSInfoServiceInterface::CWSViolationType::kNone,
      /*unpublished_long_ago=*/false,
      /*no_privacy_practice=*/false};
  CWSInfoServiceInterface::CWSInfo cws_info_malware = {
      /*is_present=*/true,
      /*is_live=*/false,
      /*last_update_time=*/base::Time::Now(),
      CWSInfoServiceInterface::CWSViolationType::kMalware,
      /*unpublished_long_ago=*/false,
      /*no_privacy_practice=*/false};
  EXPECT_CALL(mock_cws_info_service, GetCWSInfo)
      .WillOnce(testing::Return(cws_info_live))
      .WillOnce(testing::Return(cws_info_malware))
      .WillOnce(testing::Return(cws_info_not_live))
      .WillOnce(testing::Return(std::nullopt));
  // Verify that the extension is allowed when it is live in CWS.
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      normal_extension.get()));
  // Verify that the extension is ignored, i.e. allowed, when it is malware.
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      normal_extension.get()));
  // Verify that the extension is disallowed when it is not live in CWS.
  EXPECT_FALSE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      normal_extension.get()));
  // Verify that the extensions is allowed if CWS publish status is missing.
  EXPECT_TRUE(extension_management_->IsAllowedByUnpublishedAvailabilityPolicy(
      normal_extension.get()));
  SetCWSInfoService(nullptr);
}

TEST_F(ExtensionManagementServiceTest, IsFileUrlNavigationAllowed) {
  EXPECT_EQ(IsFileUrlNavigationAllowed(kTargetExtension), false);
  EXPECT_EQ(IsFileUrlNavigationAllowed(kTargetExtension2), false);

  SetExampleDictPref(base::StringPrintf(
      R"({
    "%s": {
      "file_url_navigation_allowed": true
    }
  })",
      kTargetExtension));
  EXPECT_EQ(IsFileUrlNavigationAllowed(kTargetExtension), true);
  EXPECT_EQ(IsFileUrlNavigationAllowed(kTargetExtension2), false);
}

TEST_F(ExtensionManagementServiceTest, IsAllowedByUnpackedDeveloperModePolicy) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kExtensionDisableUnsupportedDeveloper);
  scoped_refptr<const Extension> unpacked_extension =
      CreateOffstoreExtension(kNonExistingExtension);

  SetPref(false, prefs::kExtensionsUIDeveloperMode, base::Value(false));
  EXPECT_FALSE(extension_management_->IsAllowedByUnpackedDeveloperModePolicy(
      *unpacked_extension));

  SetPref(false, prefs::kExtensionsUIDeveloperMode, base::Value(true));
  EXPECT_TRUE(extension_management_->IsAllowedByUnpackedDeveloperModePolicy(
      *unpacked_extension));
}

TEST_F(ExtensionManagementServiceTest,
       ShouldBlockForceInstalledOffstoreExtension) {
  {
    // Low trust environment. Verify that extension is not allowed on
    // Windows and Mac.
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);
    scoped_refptr<const Extension> forced_extension =
        CreateForcedExtension(kTargetExtension3, Extension::NO_FLAGS);

    bool expect_blocked =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
        true;
#else
        false;
#endif
    EXPECT_EQ(extension_management_->ShouldBlockForceInstalledOffstoreExtension(
                  *forced_extension),
              expect_blocked);
  }
  {
    // High trust environment. Verify that extension is allowed.
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
    scoped_refptr<const Extension> forced_extension =
        CreateForcedExtension(kTargetExtension3, Extension::NO_FLAGS);

    EXPECT_FALSE(
        extension_management_->ShouldBlockForceInstalledOffstoreExtension(
            *forced_extension));
  }
}

// Tests the flag value indicating that extensions are blocklisted by default.
TEST_F(ExtensionAdminPolicyTest, BlocklistedByDefault) {
  EXPECT_FALSE(BlocklistedByDefault(nullptr));

  base::Value::List blocklist;
  blocklist.Append(kNonExistingExtension);
  EXPECT_FALSE(BlocklistedByDefault(&blocklist));
  blocklist.Append("*");
  EXPECT_TRUE(BlocklistedByDefault(&blocklist));

  blocklist.clear();
  blocklist.Append("*");
  EXPECT_TRUE(BlocklistedByDefault(&blocklist));
}

// Tests UserMayLoad for required extensions.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadRequired) {
  CreateExtension(ManifestLocation::kComponent);
  EXPECT_TRUE(
      UserMayLoad(nullptr, nullptr, nullptr, extension_.get(), nullptr));
  std::u16string error;
  EXPECT_TRUE(UserMayLoad(nullptr, nullptr, nullptr, extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  // Required extensions may load even if they're on the blocklist.
  base::Value::List blocklist;
  blocklist.Append(extension_->id());
  EXPECT_TRUE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), nullptr));

  blocklist.Append("*");
  EXPECT_TRUE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), nullptr));
}

// Tests UserMayLoad when no blocklist exists, or it's empty.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadNoBlocklist) {
  CreateExtension(ManifestLocation::kInternal);
  EXPECT_TRUE(
      UserMayLoad(nullptr, nullptr, nullptr, extension_.get(), nullptr));
  base::Value::List blocklist;
  EXPECT_TRUE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), nullptr));
  std::u16string error;
  EXPECT_TRUE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the allowlist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadAllowlisted) {
  CreateExtension(ManifestLocation::kInternal);

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  EXPECT_TRUE(
      UserMayLoad(nullptr, &allowlist, nullptr, extension_.get(), nullptr));

  base::Value::List blocklist;
  blocklist.Append(extension_->id());
  EXPECT_TRUE(
      UserMayLoad(nullptr, &allowlist, nullptr, extension_.get(), nullptr));
  std::u16string error;
  EXPECT_TRUE(
      UserMayLoad(nullptr, &allowlist, nullptr, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the blocklist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadBlocklisted) {
  CreateExtension(ManifestLocation::kInternal);

  // Blocklisted by default.
  base::Value::List blocklist;
  blocklist.Append("*");
  EXPECT_FALSE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), nullptr));
  std::u16string error;
  EXPECT_FALSE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  // Extension on the blocklist, with and without wildcard.
  blocklist.Append(extension_->id());
  EXPECT_FALSE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), nullptr));
  blocklist.clear();
  blocklist.Append(extension_->id());
  EXPECT_FALSE(
      UserMayLoad(&blocklist, nullptr, nullptr, extension_.get(), nullptr));

  // With a allowlist. There's no such thing as a allowlist wildcard.
  base::Value::List allowlist;
  allowlist.Append("behllobkkfkfnphdnhnkndlbkcpglgmj");
  EXPECT_FALSE(
      UserMayLoad(&blocklist, &allowlist, nullptr, extension_.get(), nullptr));
  allowlist.Append("*");
  EXPECT_FALSE(
      UserMayLoad(&blocklist, &allowlist, nullptr, extension_.get(), nullptr));
}

TEST_F(ExtensionAdminPolicyTest, UserMayLoadAllowedTypes) {
  CreateExtension(ManifestLocation::kInternal);
  EXPECT_TRUE(
      UserMayLoad(nullptr, nullptr, nullptr, extension_.get(), nullptr));

  base::Value::List allowed_types;
  EXPECT_FALSE(
      UserMayLoad(nullptr, nullptr, &allowed_types, extension_.get(), nullptr));

  allowed_types.Append(Manifest::TYPE_EXTENSION);
  EXPECT_TRUE(
      UserMayLoad(nullptr, nullptr, &allowed_types, extension_.get(), nullptr));

  CreateHostedApp(ManifestLocation::kInternal);
  EXPECT_FALSE(
      UserMayLoad(nullptr, nullptr, &allowed_types, extension_.get(), nullptr));

  CreateHostedApp(ManifestLocation::kExternalPolicyDownload);
  EXPECT_FALSE(
      UserMayLoad(nullptr, nullptr, &allowed_types, extension_.get(), nullptr));
}

TEST_F(ExtensionAdminPolicyTest, UserMayModifySettings) {
  CreateExtension(ManifestLocation::kInternal);
  EXPECT_TRUE(UserMayModifySettings(extension_.get(), nullptr));
  std::u16string error;
  EXPECT_TRUE(UserMayModifySettings(extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  CreateExtension(ManifestLocation::kExternalPolicyDownload);
  error.clear();
  EXPECT_FALSE(UserMayModifySettings(extension_.get(), nullptr));
  EXPECT_FALSE(UserMayModifySettings(extension_.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(ExtensionAdminPolicyTest, ExtensionMayModifySettings) {
  CreateExtension(ManifestLocation::kExternalPolicyDownload);
  auto external_policy_download = extension_;
  CreateExtension(ManifestLocation::kExternalPolicy);
  auto external_policy = extension_;
  CreateExtension(ManifestLocation::kExternalPref);
  auto external_pref = extension_;
  CreateExtension(ManifestLocation::kComponent);
  auto component = extension_;
  CreateExtension(ManifestLocation::kComponent);
  auto component2 = extension_;
  // Make sure that component/policy/external extensions cannot modify component
  // extensions (no extension may modify a component extension).
  EXPECT_FALSE(ExtensionMayModifySettings(external_policy_download.get(),
                                          component.get(), nullptr));
  EXPECT_FALSE(
      ExtensionMayModifySettings(component2.get(), component.get(), nullptr));
  EXPECT_FALSE(ExtensionMayModifySettings(external_pref.get(), component.get(),
                                          nullptr));

  // Only component/policy extensions *can* modify policy extensions, and eg.
  // external cannot.
  EXPECT_TRUE(ExtensionMayModifySettings(
      external_policy.get(), external_policy_download.get(), nullptr));
  EXPECT_TRUE(ExtensionMayModifySettings(
      component.get(), external_policy_download.get(), nullptr));
  EXPECT_FALSE(ExtensionMayModifySettings(
      external_pref.get(), external_policy_download.get(), nullptr));
}

TEST_F(ExtensionAdminPolicyTest, MustRemainEnabled) {
  CreateExtension(ManifestLocation::kExternalPolicyDownload);
  EXPECT_TRUE(MustRemainEnabled(extension_.get(), nullptr));
  std::u16string error;
  EXPECT_TRUE(MustRemainEnabled(extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  CreateExtension(ManifestLocation::kInternal);
  error.clear();
  EXPECT_FALSE(MustRemainEnabled(extension_.get(), nullptr));
  EXPECT_FALSE(MustRemainEnabled(extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

}  // namespace extensions
