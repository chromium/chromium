// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_parser.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

  ExtensionManagementServiceTest() {}
  ~ExtensionManagementServiceTest() override {}

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

  void SetExampleDictPref(const base::StringPiece example_dict_preference) {
    std::string error_msg;
    std::unique_ptr<base::Value> parsed =
        base::JSONReader::ReadAndReturnErrorDeprecated(
            example_dict_preference,
            base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS, NULL,
            &error_msg);
    ASSERT_TRUE(parsed && parsed->is_dict()) << error_msg;
    SetPref(true, pref_names::kExtensionManagement, std::move(parsed));
  }

  // Wrapper of ExtensionManagement::GetInstallationMode, |id| and
  // |update_url| are used to construct an Extension for testing.
  ExtensionManagement::InstallationMode GetInstallationMode(
      const std::string& id,
      const std::string& update_url) {
    scoped_refptr<const Extension> extension =
        CreateExtension(Manifest::UNPACKED, "0.1", id, update_url);
    return extension_management_->GetInstallationMode(extension.get());
  }

  // Wrapper of ExtensionManagement::GetPolicyBlockedHosts, |id| is used
  // to construct an Extension for testing.
  URLPatternSet GetPolicyBlockedHosts(const std::string& id) {
    scoped_refptr<const Extension> extension =
        CreateExtension(Manifest::UNPACKED, "0.1", id, kNonExistingUpdateUrl);
    return extension_management_->GetPolicyBlockedHosts(extension.get())
        .Clone();
  }

  // Wrapper of ExtensionManagement::GetPolicyAllowedHosts, |id| is used
  // to construct an Extension for testing.
  URLPatternSet GetPolicyAllowedHosts(const std::string& id) {
    scoped_refptr<const Extension> extension =
        CreateExtension(Manifest::UNPACKED, "0.1", id, kNonExistingUpdateUrl);
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
        CreateExtension(Manifest::UNPACKED, "0.1", id, update_url);
    return extension_management_->GetBlockedAPIPermissions(extension.get());
  }

  // Wrapper of ExtensionManagement::CheckMinimumVersion, |id| and
  // |version| are used to construct an Extension for testing.
  bool CheckMinimumVersion(const std::string& id, const std::string& version) {
    scoped_refptr<const Extension> extension =
        CreateExtension(Manifest::UNPACKED, version, id, kNonExistingUpdateUrl);
    std::string minimum_version_required;
    bool ret = extension_management_->CheckMinimumVersion(
        extension.get(), &minimum_version_required);
    EXPECT_EQ(ret, minimum_version_required.empty());
    EXPECT_EQ(ret, extension_management_->CheckMinimumVersion(extension.get(),
                                                              nullptr));
    return ret;
  }

 protected:
  // Create an extension with specified |location|, |version|, |id| and
  // |update_url|.
  scoped_refptr<const Extension> CreateExtension(
      Manifest::Location location,
      const std::string& version,
      const std::string& id,
      const std::string& update_url) {
    base::DictionaryValue manifest_dict;
    manifest_dict.SetString(manifest_keys::kName, "test");
    manifest_dict.SetString(manifest_keys::kVersion, version);
    manifest_dict.SetInteger(manifest_keys::kManifestVersion, 2);
    manifest_dict.SetString(manifest_keys::kUpdateURL, update_url);
    std::string error;
    scoped_refptr<const Extension> extension =
        Extension::Create(base::FilePath(), location, manifest_dict,
                          Extension::NO_FLAGS, id, &error);
    CHECK(extension.get()) << error;
    return extension;
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  sync_preferences::TestingPrefServiceSyncable* pref_service_;
  std::unique_ptr<ExtensionManagement> extension_management_;
};

class ExtensionAdminPolicyTest : public ExtensionManagementServiceTest {
 public:
  ExtensionAdminPolicyTest() {}
  ~ExtensionAdminPolicyTest() override {}

  void SetUpPolicyProvider() {
    provider_.reset(
        new StandardManagementPolicyProvider(extension_management_.get()));
  }

  void CreateExtension(Manifest::Location location) {
    base::DictionaryValue values;
    CreateExtensionFromValues(location, &values);
  }

  void CreateHostedApp(Manifest::Location location) {
    base::DictionaryValue values;
    values.Set(extensions::manifest_keys::kWebURLs,
               std::make_unique<base::ListValue>());
    values.SetString(extensions::manifest_keys::kLaunchWebURL,
                     "http://www.example.com");
    CreateExtensionFromValues(location, &values);
  }

  void CreateExtensionFromValues(Manifest::Location location,
                                 base::DictionaryValue* values) {
    values->SetString(extensions::manifest_keys::kName, "test");
    values->SetString(extensions::manifest_keys::kVersion, "0.1");
    values->SetInteger(extensions::manifest_keys::kManifestVersion, 2);
    std::string error;
    extension_ = Extension::Create(base::FilePath(), location, *values,
                                   Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension_.get());
  }

  // Wrappers for legacy admin policy functions, for testing purpose only.
  bool BlacklistedByDefault(const base::ListValue* blacklist);
  bool UserMayLoad(const base::ListValue* blacklist,
                   const base::ListValue* whitelist,
                   const base::DictionaryValue* forcelist,
                   const base::ListValue* allowed_types,
                   const Extension* extension,
                   base::string16* error);
  bool UserMayModifySettings(const Extension* extension, base::string16* error);
  bool ExtensionMayModifySettings(const Extension* source_extension,
                                  const Extension* extension,
                                  base::string16* error);
  bool MustRemainEnabled(const Extension* extension, base::string16* error);

 protected:
  std::unique_ptr<StandardManagementPolicyProvider> provider_;
  scoped_refptr<Extension> extension_;
};

bool ExtensionAdminPolicyTest::BlacklistedByDefault(
    const base::ListValue* blacklist) {
  SetUpPolicyProvider();
  if (blacklist)
    SetPref(true, pref_names::kInstallDenyList, blacklist->CreateDeepCopy());
  return extension_management_->BlacklistedByDefault();
}

bool ExtensionAdminPolicyTest::UserMayLoad(
    const base::ListValue* blacklist,
    const base::ListValue* whitelist,
    const base::DictionaryValue* forcelist,
    const base::ListValue* allowed_types,
    const Extension* extension,
    base::string16* error) {
  SetUpPolicyProvider();
  if (blacklist)
    SetPref(true, pref_names::kInstallDenyList, blacklist->CreateDeepCopy());
  if (whitelist)
    SetPref(true, pref_names::kInstallAllowList, whitelist->CreateDeepCopy());
  if (forcelist)
    SetPref(true, pref_names::kInstallForceList, forcelist->CreateDeepCopy());
  if (allowed_types)
    SetPref(true, pref_names::kAllowedTypes, allowed_types->CreateDeepCopy());
  return provider_->UserMayLoad(extension, error);
}

bool ExtensionAdminPolicyTest::UserMayModifySettings(const Extension* extension,
                                                     base::string16* error) {
  SetUpPolicyProvider();
  return provider_->UserMayModifySettings(extension, error);
}

bool ExtensionAdminPolicyTest::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    base::string16* error) {
  SetUpPolicyProvider();
  return provider_->ExtensionMayModifySettings(source_extension, extension,
                                               error);
}

bool ExtensionAdminPolicyTest::MustRemainEnabled(const Extension* extension,
                                                 base::string16* error) {
  SetUpPolicyProvider();
  return provider_->MustRemainEnabled(extension, error);
}

// Verify that preference controlled by legacy ExtensionInstallSources policy is
// handled well.
TEST_F(ExtensionManagementServiceTest, LegacyInstallSources) {
  base::ListValue allowed_sites_pref;
  allowed_sites_pref.AppendString("https://www.example.com/foo");
  allowed_sites_pref.AppendString("https://corp.mycompany.com/*");
  SetPref(true, pref_names::kAllowedInstallSites,
          allowed_sites_pref.CreateDeepCopy());
  const URLPatternSet& allowed_sites = ReadGlobalSettings()->install_sources;
  ASSERT_TRUE(ReadGlobalSettings()->has_restricted_install_sources);
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
  base::ListValue allowed_types_pref;
  allowed_types_pref.AppendInteger(Manifest::TYPE_THEME);
  allowed_types_pref.AppendInteger(Manifest::TYPE_USER_SCRIPT);

  SetPref(true, pref_names::kAllowedTypes, allowed_types_pref.CreateDeepCopy());
  const std::vector<Manifest::Type>& allowed_types =
      ReadGlobalSettings()->allowed_types;
  ASSERT_TRUE(ReadGlobalSettings()->has_restricted_allowed_types);
  EXPECT_EQ(allowed_types.size(), 2u);
  EXPECT_FALSE(base::Contains(allowed_types, Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_THEME));
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_USER_SCRIPT));
}

// Verify that preference controlled by legacy ExtensionInstallBlacklist policy
// is handled well.
TEST_F(ExtensionManagementServiceTest, LegacyInstallBlacklist) {
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString(kTargetExtension);

  SetPref(true, pref_names::kInstallDenyList,
          denied_list_pref.CreateDeepCopy());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Verify that preference controlled by legacy ExtensionInstallWhitelist policy
// is handled well.
TEST_F(ExtensionManagementServiceTest, LegacyInstallWhitelist) {
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString("*");
  base::ListValue allowed_list_pref;
  allowed_list_pref.AppendString(kTargetExtension);

  SetPref(true, pref_names::kInstallDenyList,
          denied_list_pref.CreateDeepCopy());
  SetPref(true, pref_names::kInstallAllowList,
          allowed_list_pref.CreateDeepCopy());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);

  // Verify that install whitelist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallAllowList);
  SetPref(false, pref_names::kInstallAllowList,
          allowed_list_pref.CreateDeepCopy());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
}

// Verify that preference controlled by legacy ExtensionInstallForcelist policy
// is handled well.
TEST_F(ExtensionManagementServiceTest, LegacyInstallForcelist) {
  base::DictionaryValue forced_list_pref;
  ExternalPolicyLoader::AddExtension(
      &forced_list_pref, kTargetExtension, kExampleUpdateUrl);

  SetPref(true, pref_names::kInstallForceList,
          forced_list_pref.CreateDeepCopy());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_FORCED);
  CheckAutomaticallyInstalledUpdateUrl(kTargetExtension, kExampleUpdateUrl);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);

  // Verify that install forcelist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallForceList);
  SetPref(false, pref_names::kInstallForceList,
          forced_list_pref.CreateDeepCopy());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
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

// Tests parsing of new dictionary preference.
TEST_F(ExtensionManagementServiceTest, PreferenceParsing) {
  SetExampleDictPref(kExampleDictPreference);

  // Verifies the installation mode settings.
  EXPECT_TRUE(extension_management_->BlacklistedByDefault());
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
  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_install_sources);
  const URLPatternSet& allowed_sites = ReadGlobalSettings()->install_sources;
  EXPECT_EQ(allowed_sites.size(), 1u);
  EXPECT_TRUE(allowed_sites.MatchesURL(GURL("http://foo.com/entry")));
  EXPECT_FALSE(allowed_sites.MatchesURL(GURL("http://bar.com/entry")));
  EXPECT_TRUE(GetPolicyBlockedHosts(kNonExistingExtension)
                  .MatchesURL(GURL("http://example.com/default")));

  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_allowed_types);
  const std::vector<Manifest::Type>& allowed_types =
      ReadGlobalSettings()->allowed_types;
  EXPECT_EQ(allowed_types.size(), 2u);
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_THEME));
  EXPECT_TRUE(base::Contains(allowed_types, Manifest::TYPE_USER_SCRIPT));

  // Verifies blocked permission list settings.
  APIPermissionSet api_permission_set;
  api_permission_set.clear();
  api_permission_set.insert(APIPermission::kFileSystem);
  api_permission_set.insert(APIPermission::kDownloads);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissionsById(kNonExistingExtension));

  api_permission_set.clear();
  api_permission_set.insert(APIPermission::kFileSystem);
  api_permission_set.insert(APIPermission::kDownloads);
  api_permission_set.insert(APIPermission::kBookmark);
  EXPECT_EQ(api_permission_set, GetBlockedAPIPermissionsById(kTargetExtension));

  api_permission_set.clear();
  api_permission_set.insert(APIPermission::kDownloads);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissionsById(kTargetExtension2));

  api_permission_set.clear();
  api_permission_set.insert(APIPermission::kFileSystem);
  api_permission_set.insert(APIPermission::kHistory);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissionsById(kTargetExtension3));

  api_permission_set.clear();
  api_permission_set.insert(APIPermission::kFileSystem);
  api_permission_set.insert(APIPermission::kBookmark);
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

  // Both settings should be enforced.
  APIPermissionSet blocked_permissions_for_update_url;
  blocked_permissions_for_update_url.insert(APIPermission::kFileSystem);
  blocked_permissions_for_update_url.insert(APIPermission::kBookmark);

  APIPermissionSet api_permission_set;

  api_permission_set = blocked_permissions_for_update_url.Clone();
  api_permission_set.insert(APIPermission::kFileSystem);
  api_permission_set.insert(APIPermission::kDownloads);
  api_permission_set.insert(APIPermission::kBookmark);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissions(kTargetExtension, kExampleUpdateUrl));

  api_permission_set = blocked_permissions_for_update_url.Clone();
  api_permission_set.insert(APIPermission::kDownloads);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissions(kTargetExtension2, kExampleUpdateUrl));

  api_permission_set = blocked_permissions_for_update_url.Clone();
  api_permission_set.insert(APIPermission::kFileSystem);
  api_permission_set.insert(APIPermission::kHistory);
  EXPECT_EQ(api_permission_set,
            GetBlockedAPIPermissions(kTargetExtension3, kExampleUpdateUrl));
}

// Tests the 'minimum_version_required' settings of extension management.
TEST_F(ExtensionManagementServiceTest, kMinimumVersionRequired) {
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "0.0"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "3.0.0"));
  EXPECT_TRUE(CheckMinimumVersion(kTargetExtension, "9999.0"));

  {
    PrefUpdater pref(pref_service_);
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
  base::ListValue allowed_sites_pref;
  allowed_sites_pref.AppendString("https://www.example.com/foo");
  SetPref(true, pref_names::kAllowedInstallSites,
          allowed_sites_pref.CreateDeepCopy());
  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_install_sources);
  EXPECT_TRUE(ReadGlobalSettings()->install_sources.MatchesURL(
      GURL("https://www.example.com/foo")));

  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_);
    updater.ClearInstallSources();
  }
  // Verifies that the new one overrides the legacy ones.
  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_install_sources);
  EXPECT_FALSE(ReadGlobalSettings()->install_sources.MatchesURL(
      GURL("https://www.example.com/foo")));

  // Updates the new dictionary preference.
  {
    PrefUpdater updater(pref_service_);
    updater.AddInstallSource("https://corp.mycompany.com/*");
  }
  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_install_sources);
  EXPECT_TRUE(ReadGlobalSettings()->install_sources.MatchesURL(
      GURL("https://corp.mycompany.com/entry")));
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionAllowedTypes policy.
TEST_F(ExtensionManagementServiceTest, NewAllowedTypes) {
  // Set the legacy preference, and verifies that it works.
  base::ListValue allowed_types_pref;
  allowed_types_pref.AppendInteger(Manifest::TYPE_USER_SCRIPT);
  SetPref(true, pref_names::kAllowedTypes, allowed_types_pref.CreateDeepCopy());
  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_allowed_types);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types.size(), 1u);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types[0], Manifest::TYPE_USER_SCRIPT);

  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_);
    updater.ClearAllowedTypes();
  }
  // Verifies that the new one overrides the legacy ones.
  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_allowed_types);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types.size(), 0u);

  // Updates the new dictionary preference.
  {
    PrefUpdater updater(pref_service_);
    updater.AddAllowedType("theme");
  }
  EXPECT_TRUE(ReadGlobalSettings()->has_restricted_allowed_types);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types.size(), 1u);
  EXPECT_EQ(ReadGlobalSettings()->allowed_types[0], Manifest::TYPE_THEME);
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionInstallBlacklist policy.
TEST_F(ExtensionManagementServiceTest, NewInstallBlacklist) {
  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_);
    updater.SetBlacklistedByDefault(false);  // Allowed by default.
    updater.SetIndividualExtensionInstallationAllowed(kTargetExtension, false);
    updater.ClearPerExtensionSettings(kTargetExtension2);
  }
  EXPECT_FALSE(extension_management_->BlacklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);

  // Set legacy preference.
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString("*");
  denied_list_pref.AppendString(kTargetExtension2);
  SetPref(true, pref_names::kInstallDenyList,
          denied_list_pref.CreateDeepCopy());

  base::ListValue allowed_list_pref;
  allowed_list_pref.AppendString(kTargetExtension);
  SetPref(true, pref_names::kInstallAllowList,
          allowed_list_pref.CreateDeepCopy());

  // Verifies that the new one have higher priority over the legacy ones.
  EXPECT_FALSE(extension_management_->BlacklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kTargetExtension2),
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Tests functionality of new preference as to deprecate legacy
// ExtensionInstallWhitelist policy.
TEST_F(ExtensionManagementServiceTest, NewInstallWhitelist) {
  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_);
    updater.SetBlacklistedByDefault(true);  // Disallowed by default.
    updater.SetIndividualExtensionInstallationAllowed(kTargetExtension, true);
    updater.ClearPerExtensionSettings(kTargetExtension2);
  }
  EXPECT_TRUE(extension_management_->BlacklistedByDefault());
  EXPECT_EQ(GetInstallationModeById(kTargetExtension),
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(GetInstallationModeById(kNonExistingExtension),
            ExtensionManagement::INSTALLATION_BLOCKED);

  // Set legacy preference.
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString(kTargetExtension);
  SetPref(true, pref_names::kInstallDenyList,
          denied_list_pref.CreateDeepCopy());

  base::ListValue allowed_list_pref;
  allowed_list_pref.AppendString(kTargetExtension2);
  SetPref(true, pref_names::kInstallAllowList,
          allowed_list_pref.CreateDeepCopy());

  // Verifies that the new one have higher priority over the legacy ones.
  EXPECT_TRUE(extension_management_->BlacklistedByDefault());
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
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString(kTargetExtension);
  SetPref(true, pref_names::kInstallDenyList,
          denied_list_pref.CreateDeepCopy());

  // Set the new dictionary preference.
  {
    PrefUpdater updater(pref_service_);
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

  // BlacklistedByDefault() is true in example preference.
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyAllowed(allowed));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyAllowed(forced));
  EXPECT_TRUE(
      extension_management_->IsInstallationExplicitlyAllowed(recommended));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyAllowed(blocked));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyAllowed(removed));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyAllowed(not_specified));

  // Set BlacklistedByDefault() to false.
  PrefUpdater pref(pref_service_);
  pref.SetBlacklistedByDefault(false);

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

  // BlacklistedByDefault() is true in example preference.
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(allowed));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(forced));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(recommended));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(blocked));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(removed));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(not_specified));

  PrefUpdater pref(pref_service_);
  pref.SetBlacklistedByDefault(false);

  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(allowed));
  EXPECT_FALSE(extension_management_->IsInstallationExplicitlyBlocked(forced));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(recommended));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(blocked));
  EXPECT_TRUE(extension_management_->IsInstallationExplicitlyBlocked(removed));
  EXPECT_FALSE(
      extension_management_->IsInstallationExplicitlyBlocked(not_specified));
}

#if !defined(OS_CHROMEOS)
TEST_F(ExtensionManagementServiceTest, CloudReportingEnabledPolicy) {
  // Enables the policy put the extension into forced list.
  SetPref(true, prefs::kCloudReportingEnabled,
          std::make_unique<base::Value>(true));
  CheckAutomaticallyInstalledUpdateUrl(
      extension_misc::kCloudReportingExtensionId,
      extension_urls::kChromeWebstoreUpdateURL);
  EXPECT_EQ(
      ExtensionManagement::INSTALLATION_FORCED,
      GetInstallationModeById(extension_misc::kCloudReportingExtensionId));

  // Disabling the policy should remove the extension from the forced list.
  RemovePref(true, prefs::kCloudReportingEnabled);
  EXPECT_EQ(
      ExtensionManagement::INSTALLATION_ALLOWED,
      GetInstallationModeById(extension_misc::kCloudReportingExtensionId));

  // Recommended policy does not force install the policy.
  pref_service_->SetRecommendedPref(prefs::kCloudReportingEnabled,
                                    std::make_unique<base::Value>(true));
  EXPECT_EQ(
      ExtensionManagement::INSTALLATION_ALLOWED,
      GetInstallationModeById(extension_misc::kCloudReportingExtensionId));
}

TEST_F(ExtensionManagementServiceTest,
       CloudReportingEnabledPolicyOverridesBlacklist) {
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString(extension_misc::kCloudReportingExtensionId);
  SetPref(true, pref_names::kInstallDenyList,
          denied_list_pref.CreateDeepCopy());
  EXPECT_EQ(
      ExtensionManagement::INSTALLATION_BLOCKED,
      GetInstallationModeById(extension_misc::kCloudReportingExtensionId));
  SetPref(true, prefs::kCloudReportingEnabled,
          std::make_unique<base::Value>(true));
  EXPECT_EQ(
      ExtensionManagement::INSTALLATION_FORCED,
      GetInstallationModeById(extension_misc::kCloudReportingExtensionId));
}

TEST_F(ExtensionManagementServiceTest,
       CloudReportingEnabledPolicyOverridesAllowedTypes) {
  scoped_refptr<const Extension> extension =
      CreateExtension(Manifest::EXTERNAL_POLICY, "1.0",
                      extension_misc::kCloudReportingExtensionId,
                      extension_urls::kChromeWebstoreUpdateURL);
  StandardManagementPolicyProvider provider(extension_management_.get());

  base::ListValue allowed_type_pref;
  base::string16 error;
  allowed_type_pref.AppendInteger(Manifest::TYPE_THEME);
  SetPref(true, pref_names::kAllowedTypes, allowed_type_pref.CreateDeepCopy());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));

  SetPref(true, prefs::kCloudReportingEnabled,
          std::make_unique<base::Value>(true));

  EXPECT_TRUE(provider.UserMayLoad(extension.get(), nullptr));
}

TEST_F(ExtensionManagementServiceTest,
       CloudReportingenabledOverridesExtensionSettings) {
  SetExampleDictPref(R"({
        "oempjldejiginopiohodkdoklcjklbaa": {
          "installation_mode": "allowed",
          "blocked_permissions": ["bookmarks"],
          "minimum_version_required": "100.0",
          "runtime_blocked_hosts": ["https://a.com"],
          "runtime_allowed_hosts": ["https://b.com"],
          "update_url": "http://example.com/update_url",
        },
        "update_url:https://clients2.google.com/service/update2/crx": {
          "blocked_permissions": ["downloads"],
        },
        "update_url:http://example.com/update_url": {
          "blocked_permissions": ["downloads"],
        }
      })");

  EXPECT_EQ(
      ExtensionManagement::INSTALLATION_ALLOWED,
      GetInstallationModeById(extension_misc::kCloudReportingExtensionId));
  EXPECT_EQ(2u,
            GetBlockedAPIPermissions(extension_misc::kCloudReportingExtensionId,
                                     extension_urls::kChromeWebstoreUpdateURL)
                .size());
  EXPECT_FALSE(
      CheckMinimumVersion(extension_misc::kCloudReportingExtensionId, "99.0"));
  EXPECT_EQ(
      1u,
      GetPolicyBlockedHosts(extension_misc::kCloudReportingExtensionId).size());
  EXPECT_EQ(
      1u,
      GetPolicyAllowedHosts(extension_misc::kCloudReportingExtensionId).size());

  SetPref(true, prefs::kCloudReportingEnabled,
          std::make_unique<base::Value>(true));
  CheckAutomaticallyInstalledUpdateUrl(
      extension_misc::kCloudReportingExtensionId,
      extension_urls::kChromeWebstoreUpdateURL);
  EXPECT_EQ(
      ExtensionManagement::INSTALLATION_FORCED,
      GetInstallationModeById(extension_misc::kCloudReportingExtensionId));
  EXPECT_TRUE(
      GetBlockedAPIPermissions(extension_misc::kCloudReportingExtensionId,
                               extension_urls::kChromeWebstoreUpdateURL)
          .empty());
  EXPECT_TRUE(
      CheckMinimumVersion(extension_misc::kCloudReportingExtensionId, "99.0"));
  EXPECT_TRUE(GetPolicyBlockedHosts(extension_misc::kCloudReportingExtensionId)
                  .is_empty());
  EXPECT_TRUE(GetPolicyAllowedHosts(extension_misc::kCloudReportingExtensionId)
                  .is_empty());
}
#endif

TEST_F(ExtensionManagementServiceTest,
       ExtensionsAreBlockedByDefaultForExtensionRequest) {
  // When extension request policy is set to true, all extensions are blocked by
  // default.
  SetPref(true, prefs::kCloudExtensionRequestEnabled,
          std::make_unique<base::Value>(true));
  EXPECT_TRUE(extension_management_->BlacklistedByDefault());
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

// Tests the flag value indicating that extensions are blacklisted by default.
TEST_F(ExtensionAdminPolicyTest, BlacklistedByDefault) {
  EXPECT_FALSE(BlacklistedByDefault(NULL));

  base::ListValue blacklist;
  blacklist.AppendString(kNonExistingExtension);
  EXPECT_FALSE(BlacklistedByDefault(&blacklist));
  blacklist.AppendString("*");
  EXPECT_TRUE(BlacklistedByDefault(&blacklist));

  blacklist.Clear();
  blacklist.AppendString("*");
  EXPECT_TRUE(BlacklistedByDefault(&blacklist));
}

// Tests UserMayLoad for required extensions.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadRequired) {
  CreateExtension(Manifest::COMPONENT);
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  // Required extensions may load even if they're on the blacklist.
  base::ListValue blacklist;
  blacklist.AppendString(extension_->id());
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));

  blacklist.AppendString("*");
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
}

// Tests UserMayLoad when no blacklist exists, or it's empty.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadNoBlacklist) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));
  base::ListValue blacklist;
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the whitelist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadWhitelisted) {
  CreateExtension(Manifest::INTERNAL);

  base::ListValue whitelist;
  whitelist.AppendString(extension_->id());
  EXPECT_TRUE(
      UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(), NULL));

  base::ListValue blacklist;
  blacklist.AppendString(extension_->id());
  EXPECT_TRUE(
      UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(
      UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the blacklist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadBlacklisted) {
  CreateExtension(Manifest::INTERNAL);

  // Blacklisted by default.
  base::ListValue blacklist;
  blacklist.AppendString("*");
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  // Extension on the blacklist, with and without wildcard.
  blacklist.AppendString(extension_->id());
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
  blacklist.Clear();
  blacklist.AppendString(extension_->id());
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));

  // With a whitelist. There's no such thing as a whitelist wildcard.
  base::ListValue whitelist;
  whitelist.AppendString("behllobkkfkfnphdnhnkndlbkcpglgmj");
  EXPECT_FALSE(
      UserMayLoad(&blacklist, &whitelist, NULL, NULL, extension_.get(), NULL));
  whitelist.AppendString("*");
  EXPECT_FALSE(
      UserMayLoad(&blacklist, &whitelist, NULL, NULL, extension_.get(), NULL));
}

TEST_F(ExtensionAdminPolicyTest, UserMayLoadAllowedTypes) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));

  base::ListValue allowed_types;
  EXPECT_FALSE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));

  allowed_types.AppendInteger(Manifest::TYPE_EXTENSION);
  EXPECT_TRUE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));

  CreateHostedApp(Manifest::INTERNAL);
  EXPECT_FALSE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));

  CreateHostedApp(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  EXPECT_FALSE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));
}

TEST_F(ExtensionAdminPolicyTest, UserMayModifySettings) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(UserMayModifySettings(extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(UserMayModifySettings(extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  error.clear();
  EXPECT_FALSE(UserMayModifySettings(extension_.get(), NULL));
  EXPECT_FALSE(UserMayModifySettings(extension_.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(ExtensionAdminPolicyTest, ExtensionMayModifySettings) {
  CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  auto external_policy_download = extension_;
  CreateExtension(Manifest::EXTERNAL_POLICY);
  auto external_policy = extension_;
  CreateExtension(Manifest::EXTERNAL_PREF);
  auto external_pref = extension_;
  CreateExtension(Manifest::COMPONENT);
  auto component = extension_;
  CreateExtension(Manifest::COMPONENT);
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
  CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  EXPECT_TRUE(MustRemainEnabled(extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(MustRemainEnabled(extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  CreateExtension(Manifest::INTERNAL);
  error.clear();
  EXPECT_FALSE(MustRemainEnabled(extension_.get(), NULL));
  EXPECT_FALSE(MustRemainEnabled(extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

}  // namespace extensions
