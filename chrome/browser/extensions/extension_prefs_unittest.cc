// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_prefs_unittest.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/mock_notification_observer.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/install_flag.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"

using base::Time;
using base::TimeDelta;

namespace extensions {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

ExtensionPrefsTest::ExtensionPrefsTest()
    : prefs_(base::ThreadTaskRunnerHandle::Get()) {}

ExtensionPrefsTest::~ExtensionPrefsTest() {
}

void ExtensionPrefsTest::RegisterPreferences(
    user_prefs::PrefRegistrySyncable* registry) {}

void ExtensionPrefsTest::SetUp() {
  RegisterPreferences(prefs_.pref_registry().get());
  Initialize();
}

void ExtensionPrefsTest::TearDown() {
  Verify();

  // Reset ExtensionPrefs, and re-verify.
  prefs_.ResetPrefRegistry();
  RegisterPreferences(prefs_.pref_registry().get());
  prefs_.RecreateExtensionPrefs();
  Verify();
  prefs_.pref_service()->CommitPendingWrite();
  base::RunLoop().RunUntilIdle();
}

// Tests the LastPingDay/SetLastPingDay functions.
class ExtensionPrefsLastPingDay : public ExtensionPrefsTest {
 public:
  ExtensionPrefsLastPingDay()
      : extension_time_(Time::Now() - TimeDelta::FromHours(4)),
        blacklist_time_(Time::Now() - TimeDelta::FromHours(2)) {}

  void Initialize() override {
    extension_id_ = prefs_.AddExtensionAndReturnId("last_ping_day");
    EXPECT_TRUE(prefs()->LastPingDay(extension_id_).is_null());
    prefs()->SetLastPingDay(extension_id_, extension_time_);
    prefs()->SetBlacklistLastPingDay(blacklist_time_);
  }

  void Verify() override {
    Time result = prefs()->LastPingDay(extension_id_);
    EXPECT_FALSE(result.is_null());
    EXPECT_TRUE(result == extension_time_);
    result = prefs()->BlacklistLastPingDay();
    EXPECT_FALSE(result.is_null());
    EXPECT_TRUE(result == blacklist_time_);
  }

 private:
  Time extension_time_;
  Time blacklist_time_;
  std::string extension_id_;
};
TEST_F(ExtensionPrefsLastPingDay, LastPingDay) {}

// Tests the GetToolbarOrder/SetToolbarOrder functions.
class ExtensionPrefsToolbarOrder : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    list_.push_back(prefs_.AddExtensionAndReturnId("1"));
    list_.push_back(prefs_.AddExtensionAndReturnId("2"));
    list_.push_back(prefs_.AddExtensionAndReturnId("3"));
    ExtensionIdList before_list = prefs()->GetToolbarOrder();
    EXPECT_TRUE(before_list.empty());
    prefs()->SetToolbarOrder(list_);
  }

  void Verify() override {
    ExtensionIdList result = prefs()->GetToolbarOrder();
    ASSERT_EQ(list_, result);
  }

 private:
  ExtensionIdList list_;
};
TEST_F(ExtensionPrefsToolbarOrder, ToolbarOrder) {}

// Tests the IsExtensionDisabled/SetExtensionState functions.
class ExtensionPrefsExtensionState : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension = prefs_.AddExtension("test");
    prefs()->SetExtensionDisabled(extension->id(),
                                  disable_reason::DISABLE_USER_ACTION);
  }

  void Verify() override {
    EXPECT_TRUE(prefs()->IsExtensionDisabled(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsExtensionState, ExtensionState) {}

class ExtensionPrefsEscalatePermissions : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension = prefs_.AddExtension("test");
    prefs()->SetExtensionDisabled(extension->id(),
                                  disable_reason::DISABLE_PERMISSIONS_INCREASE);
  }

  void Verify() override {
    EXPECT_TRUE(prefs()->DidExtensionEscalatePermissions(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsEscalatePermissions, EscalatePermissions) {}

// Tests the AddGrantedPermissions / GetGrantedPermissions functions.
class ExtensionPrefsGrantedPermissions : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

    extension_id_ = prefs_.AddExtensionAndReturnId("test");

    api_perm_set1_.insert(APIPermission::kTab);
    api_perm_set1_.insert(APIPermission::kBookmark);
    std::unique_ptr<APIPermission> permission(
        permission_info->CreateAPIPermission());
    {
      std::unique_ptr<base::ListValue> value(new base::ListValue());
      value->AppendString("tcp-connect:*.example.com:80");
      value->AppendString("udp-bind::8080");
      value->AppendString("udp-send-to::8888");
      ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
    }
    api_perm_set1_.insert(permission.release());

    api_perm_set2_.insert(APIPermission::kHistory);

    AddPattern(&ehost_perm_set1_, "http://*.google.com/*");
    AddPattern(&ehost_perm_set1_, "http://example.com/*");
    AddPattern(&ehost_perm_set1_, "chrome://favicon/*");

    AddPattern(&ehost_perm_set2_, "https://*.google.com/*");
    // with duplicate:
    AddPattern(&ehost_perm_set2_, "http://*.google.com/*");

    AddPattern(&shost_perm_set1_, "http://reddit.com/r/test/*");
    AddPattern(&shost_perm_set2_, "http://reddit.com/r/test/*");
    AddPattern(&shost_perm_set2_, "http://somesite.com/*");
    AddPattern(&shost_perm_set2_, "http://example.com/*");

    APIPermissionSet expected_apis = api_perm_set1_;

    AddPattern(&ehost_permissions_, "http://*.google.com/*");
    AddPattern(&ehost_permissions_, "http://example.com/*");
    AddPattern(&ehost_permissions_, "chrome://favicon/*");
    AddPattern(&ehost_permissions_, "https://*.google.com/*");

    AddPattern(&shost_permissions_, "http://reddit.com/r/test/*");
    AddPattern(&shost_permissions_, "http://somesite.com/*");
    AddPattern(&shost_permissions_, "http://example.com/*");

    APIPermissionSet empty_set;
    ManifestPermissionSet empty_manifest_permissions;
    URLPatternSet empty_extent;

    // Make sure both granted api and host permissions start empty.
    EXPECT_TRUE(prefs()->GetGrantedPermissions(extension_id_)->IsEmpty());

    {
      // Add part of the api permissions.
      prefs()->AddGrantedPermissions(
          extension_id_,
          PermissionSet(api_perm_set1_, empty_manifest_permissions,
                        empty_extent, empty_extent));
      std::unique_ptr<const PermissionSet> granted_permissions =
          prefs()->GetGrantedPermissions(extension_id_);
      EXPECT_TRUE(granted_permissions.get());
      EXPECT_FALSE(granted_permissions->IsEmpty());
      EXPECT_EQ(expected_apis, granted_permissions->apis());
      EXPECT_TRUE(granted_permissions->effective_hosts().is_empty());
    }

    {
      // Add part of the explicit host permissions.
      prefs()->AddGrantedPermissions(
          extension_id_, PermissionSet(empty_set, empty_manifest_permissions,
                                       ehost_perm_set1_, empty_extent));
      std::unique_ptr<const PermissionSet> granted_permissions =
          prefs()->GetGrantedPermissions(extension_id_);
      EXPECT_FALSE(granted_permissions->IsEmpty());
      EXPECT_EQ(expected_apis, granted_permissions->apis());
      EXPECT_EQ(ehost_perm_set1_, granted_permissions->explicit_hosts());
      EXPECT_EQ(ehost_perm_set1_, granted_permissions->effective_hosts());
    }

    {
      // Add part of the scriptable host permissions.
      prefs()->AddGrantedPermissions(
          extension_id_, PermissionSet(empty_set, empty_manifest_permissions,
                                       empty_extent, shost_perm_set1_));
      std::unique_ptr<const PermissionSet> granted_permissions =
          prefs()->GetGrantedPermissions(extension_id_);
      EXPECT_FALSE(granted_permissions->IsEmpty());
      EXPECT_EQ(expected_apis, granted_permissions->apis());
      EXPECT_EQ(ehost_perm_set1_, granted_permissions->explicit_hosts());
      EXPECT_EQ(shost_perm_set1_, granted_permissions->scriptable_hosts());

      effective_permissions_ =
          URLPatternSet::CreateUnion(ehost_perm_set1_, shost_perm_set1_);
      EXPECT_EQ(effective_permissions_, granted_permissions->effective_hosts());
    }

    {
      // Add the rest of the permissions.
      APIPermissionSet::Union(expected_apis, api_perm_set2_, &api_permissions_);
      prefs()->AddGrantedPermissions(
          extension_id_,
          PermissionSet(api_perm_set2_, empty_manifest_permissions,
                        ehost_perm_set2_, shost_perm_set2_));

      std::unique_ptr<const PermissionSet> granted_permissions =
          prefs()->GetGrantedPermissions(extension_id_);
      EXPECT_TRUE(granted_permissions.get());
      EXPECT_FALSE(granted_permissions->IsEmpty());
      EXPECT_EQ(api_permissions_, granted_permissions->apis());
      EXPECT_EQ(ehost_permissions_, granted_permissions->explicit_hosts());
      EXPECT_EQ(shost_permissions_, granted_permissions->scriptable_hosts());
      effective_permissions_ =
          URLPatternSet::CreateUnion(ehost_permissions_, shost_permissions_);
      EXPECT_EQ(effective_permissions_, granted_permissions->effective_hosts());
    }
  }

  void Verify() override {
    std::unique_ptr<const PermissionSet> permissions =
        prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_TRUE(permissions.get());
    EXPECT_EQ(api_permissions_, permissions->apis());
    EXPECT_EQ(ehost_permissions_,
              permissions->explicit_hosts());
    EXPECT_EQ(shost_permissions_,
              permissions->scriptable_hosts());
  }

 private:
  std::string extension_id_;
  APIPermissionSet api_perm_set1_;
  APIPermissionSet api_perm_set2_;
  URLPatternSet ehost_perm_set1_;
  URLPatternSet ehost_perm_set2_;
  URLPatternSet shost_perm_set1_;
  URLPatternSet shost_perm_set2_;

  APIPermissionSet api_permissions_;
  URLPatternSet ehost_permissions_;
  URLPatternSet shost_permissions_;
  URLPatternSet effective_permissions_;
};
TEST_F(ExtensionPrefsGrantedPermissions, GrantedPermissions) {}

// Tests the SetActivePermissions / GetActivePermissions functions.
class ExtensionPrefsActivePermissions : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension_id_ = prefs_.AddExtensionAndReturnId("test");

    APIPermissionSet api_perms;
    api_perms.insert(APIPermission::kTab);
    api_perms.insert(APIPermission::kBookmark);
    api_perms.insert(APIPermission::kHistory);

    ManifestPermissionSet empty_manifest_permissions;

    URLPatternSet ehosts;
    AddPattern(&ehosts, "http://*.google.com/*");
    AddPattern(&ehosts, "http://example.com/*");
    AddPattern(&ehosts, "chrome://favicon/*");

    URLPatternSet shosts;
    AddPattern(&shosts, "https://*.google.com/*");
    AddPattern(&shosts, "http://reddit.com/r/test/*");

    active_perms_.reset(new PermissionSet(api_perms, empty_manifest_permissions,
                                          ehosts, shosts));

    // Make sure the active permissions start empty.
    std::unique_ptr<const PermissionSet> active =
        prefs()->GetActivePermissions(extension_id_);
    EXPECT_TRUE(active->IsEmpty());

    // Set the active permissions.
    prefs()->SetActivePermissions(extension_id_, *active_perms_);
    active = prefs()->GetActivePermissions(extension_id_);
    EXPECT_EQ(active_perms_->apis(), active->apis());
    EXPECT_EQ(active_perms_->explicit_hosts(), active->explicit_hosts());
    EXPECT_EQ(active_perms_->scriptable_hosts(), active->scriptable_hosts());
    EXPECT_EQ(*active_perms_, *active);

    // Reset the active permissions.
    active_perms_ = std::make_unique<PermissionSet>();
    prefs()->SetActivePermissions(extension_id_, *active_perms_);
    active = prefs()->GetActivePermissions(extension_id_);
    EXPECT_EQ(*active_perms_, *active);
  }

  void Verify() override {
    std::unique_ptr<const PermissionSet> permissions =
        prefs()->GetActivePermissions(extension_id_);
    EXPECT_EQ(*active_perms_, *permissions);
  }

 private:
  std::string extension_id_;
  std::unique_ptr<const PermissionSet> active_perms_;
};
TEST_F(ExtensionPrefsActivePermissions, SetAndGetActivePermissions) {}

// Tests the GetVersionString function.
class ExtensionPrefsVersionString : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension = prefs_.AddExtension("test");
    EXPECT_EQ("0.1", prefs()->GetVersionString(extension->id()));
    prefs()->OnExtensionUninstalled(extension->id(),
                                    Manifest::INTERNAL, false);
  }

  void Verify() override {
    EXPECT_EQ("", prefs()->GetVersionString(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsVersionString, VersionString) {}

class ExtensionPrefsAcknowledgment : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    not_installed_id_ = "pghjnghklobnfoidcldiidjjjhkeeaoi";

    // Install some extensions.
    for (int i = 0; i < 5; i++) {
      std::string name = "test" + base::IntToString(i);
      extensions_.push_back(prefs_.AddExtension(name));
    }
    EXPECT_EQ(NULL,
              prefs()->GetInstalledExtensionInfo(not_installed_id_).get());

    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      std::string id = (*iter)->id();
      EXPECT_FALSE(prefs()->IsExternalExtensionAcknowledged(id));
      EXPECT_FALSE(prefs()->IsBlacklistedExtensionAcknowledged(id));
      if (external_id_.empty()) {
        external_id_ = id;
        continue;
      }
      if (blacklisted_id_.empty()) {
        blacklisted_id_ = id;
        continue;
      }
    }
    // For each type of acknowledgment, acknowledge one installed and one
    // not-installed extension id.
    prefs()->AcknowledgeExternalExtension(external_id_);
    prefs()->AcknowledgeBlacklistedExtension(blacklisted_id_);
    prefs()->AcknowledgeExternalExtension(not_installed_id_);
    prefs()->AcknowledgeBlacklistedExtension(not_installed_id_);
  }

  void Verify() override {
    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      std::string id = (*iter)->id();
      if (id == external_id_) {
        EXPECT_TRUE(prefs()->IsExternalExtensionAcknowledged(id));
      } else {
        EXPECT_FALSE(prefs()->IsExternalExtensionAcknowledged(id));
      }
      if (id == blacklisted_id_) {
        EXPECT_TRUE(prefs()->IsBlacklistedExtensionAcknowledged(id));
      } else {
        EXPECT_FALSE(prefs()->IsBlacklistedExtensionAcknowledged(id));
      }
    }
    EXPECT_TRUE(prefs()->IsExternalExtensionAcknowledged(not_installed_id_));
    EXPECT_TRUE(prefs()->IsBlacklistedExtensionAcknowledged(not_installed_id_));
  }

 private:
  ExtensionList extensions_;

  std::string not_installed_id_;
  std::string external_id_;
  std::string blacklisted_id_;
};
TEST_F(ExtensionPrefsAcknowledgment, Acknowledgment) {}

// Tests the idle install information functions.
class ExtensionPrefsDelayedInstallInfo : public ExtensionPrefsTest {
 public:
  // Sets idle install information for one test extension.
  void SetIdleInfo(const std::string& id, int num) {
    base::DictionaryValue manifest;
    manifest.SetString(manifest_keys::kName, "test");
    manifest.SetString(manifest_keys::kVersion, "1." + base::IntToString(num));
    manifest.SetInteger(manifest_keys::kManifestVersion, 2);
    base::FilePath path =
        prefs_.extensions_dir().AppendASCII(base::IntToString(num));
    std::string errors;
    scoped_refptr<Extension> extension = Extension::Create(
        path, Manifest::INTERNAL, manifest, Extension::NO_FLAGS, id, &errors);
    ASSERT_TRUE(extension.get()) << errors;
    ASSERT_EQ(id, extension->id());
    prefs()->SetDelayedInstallInfo(extension.get(),
                                   Extension::ENABLED,
                                   kInstallFlagNone,
                                   ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE,
                                   syncer::StringOrdinal(),
                                   std::string());
  }

  // Verifies that we get back expected idle install information previously
  // set by SetIdleInfo.
  void VerifyIdleInfo(const std::string& id, int num) {
    std::unique_ptr<ExtensionInfo> info(prefs()->GetDelayedInstallInfo(id));
    ASSERT_TRUE(info);
    std::string version;
    ASSERT_TRUE(info->extension_manifest->GetString("version", &version));
    ASSERT_EQ("1." + base::IntToString(num), version);
    ASSERT_EQ(base::IntToString(num),
              info->extension_path.BaseName().MaybeAsASCII());
  }

  bool HasInfoForId(ExtensionPrefs::ExtensionsInfo* info,
                    const std::string& id) {
    for (size_t i = 0; i < info->size(); ++i) {
      if (info->at(i)->extension_id == id)
        return true;
    }
    return false;
  }

  void Initialize() override {
    base::PathService::Get(chrome::DIR_TEST_DATA, &basedir_);
    now_ = Time::Now();
    id1_ = prefs_.AddExtensionAndReturnId("1");
    id2_ = prefs_.AddExtensionAndReturnId("2");
    id3_ = prefs_.AddExtensionAndReturnId("3");
    id4_ = prefs_.AddExtensionAndReturnId("4");

    // Set info for two extensions, then remove it.
    SetIdleInfo(id1_, 1);
    SetIdleInfo(id2_, 2);
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    std::unique_ptr<ExtensionPrefs::ExtensionsInfo> info(
        prefs()->GetAllDelayedInstallInfo());
    EXPECT_EQ(2u, info->size());
    EXPECT_TRUE(HasInfoForId(info.get(), id1_));
    EXPECT_TRUE(HasInfoForId(info.get(), id2_));
    prefs()->RemoveDelayedInstallInfo(id1_);
    prefs()->RemoveDelayedInstallInfo(id2_);
    info = prefs()->GetAllDelayedInstallInfo();
    EXPECT_TRUE(info->empty());

    // Try getting/removing info for an id that used to have info set.
    EXPECT_FALSE(prefs()->GetDelayedInstallInfo(id1_));
    EXPECT_FALSE(prefs()->RemoveDelayedInstallInfo(id1_));

    // Try getting/removing info for an id that has not yet had any info set.
    EXPECT_FALSE(prefs()->GetDelayedInstallInfo(id3_));
    EXPECT_FALSE(prefs()->RemoveDelayedInstallInfo(id3_));

    // Set info for 4 extensions, then remove for one of them.
    SetIdleInfo(id1_, 1);
    SetIdleInfo(id2_, 2);
    SetIdleInfo(id3_, 3);
    SetIdleInfo(id4_, 4);
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    VerifyIdleInfo(id3_, 3);
    VerifyIdleInfo(id4_, 4);
    prefs()->RemoveDelayedInstallInfo(id3_);
  }

  void Verify() override {
    // Make sure the info for the 3 extensions we expect is present.
    std::unique_ptr<ExtensionPrefs::ExtensionsInfo> info(
        prefs()->GetAllDelayedInstallInfo());
    EXPECT_EQ(3u, info->size());
    EXPECT_TRUE(HasInfoForId(info.get(), id1_));
    EXPECT_TRUE(HasInfoForId(info.get(), id2_));
    EXPECT_TRUE(HasInfoForId(info.get(), id4_));
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    VerifyIdleInfo(id4_, 4);

    // Make sure there isn't info the for the one extension id we removed.
    EXPECT_FALSE(prefs()->GetDelayedInstallInfo(id3_));
  }

 protected:
  Time now_;
  base::FilePath basedir_;
  std::string id1_;
  std::string id2_;
  std::string id3_;
  std::string id4_;
};
TEST_F(ExtensionPrefsDelayedInstallInfo, DelayedInstallInfo) {}

// Tests the FinishDelayedInstallInfo function.
class ExtensionPrefsFinishDelayedInstallInfo : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    base::DictionaryValue dictionary;
    dictionary.SetString(manifest_keys::kName, "test");
    dictionary.SetString(manifest_keys::kVersion, "0.1");
    dictionary.SetInteger(manifest_keys::kManifestVersion, 2);
    dictionary.SetString(manifest_keys::kBackgroundPage, "background.html");
    scoped_refptr<Extension> extension =
        prefs_.AddExtensionWithManifest(dictionary, Manifest::INTERNAL);
    id_ = extension->id();


    // Set idle info
    base::DictionaryValue manifest;
    manifest.SetString(manifest_keys::kName, "test");
    manifest.SetString(manifest_keys::kVersion, "0.2");
    manifest.SetInteger(manifest_keys::kManifestVersion, 2);
    std::unique_ptr<base::ListValue> scripts(new base::ListValue);
    scripts->AppendString("test.js");
    manifest.Set(manifest_keys::kBackgroundScripts, std::move(scripts));
    base::FilePath path =
        prefs_.extensions_dir().AppendASCII("test_0.2");
    std::string errors;
    scoped_refptr<Extension> new_extension = Extension::Create(
        path, Manifest::INTERNAL, manifest, Extension::NO_FLAGS, id_, &errors);
    ASSERT_TRUE(new_extension.get()) << errors;
    ASSERT_EQ(id_, new_extension->id());
    prefs()->SetDelayedInstallInfo(new_extension.get(),
                                   Extension::ENABLED,
                                   kInstallFlagNone,
                                   ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE,
                                   syncer::StringOrdinal(),
                                   "Param");

    // Finish idle installation
    ASSERT_TRUE(prefs()->FinishDelayedInstallInfo(id_));
  }

  void Verify() override {
    EXPECT_FALSE(prefs()->GetDelayedInstallInfo(id_));
    EXPECT_EQ(std::string("Param"), prefs()->GetInstallParam(id_));

    const base::DictionaryValue* manifest;
    ASSERT_TRUE(prefs()->ReadPrefAsDictionary(id_, "manifest", &manifest));
    ASSERT_TRUE(manifest);
    std::string value;
    EXPECT_TRUE(manifest->GetString(manifest_keys::kName, &value));
    EXPECT_EQ("test", value);
    EXPECT_TRUE(manifest->GetString(manifest_keys::kVersion, &value));
    EXPECT_EQ("0.2", value);
    EXPECT_FALSE(manifest->GetString(manifest_keys::kBackgroundPage, &value));
    const base::ListValue* scripts;
    ASSERT_TRUE(manifest->GetList(manifest_keys::kBackgroundScripts, &scripts));
    EXPECT_EQ(1u, scripts->GetSize());
  }

 protected:
  std::string id_;
};
TEST_F(ExtensionPrefsFinishDelayedInstallInfo, FinishDelayedInstallInfo) {}

class ExtensionPrefsOnExtensionInstalled : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension_ = prefs_.AddExtension("on_extension_installed");
    EXPECT_FALSE(prefs()->IsExtensionDisabled(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(),
                                  Extension::DISABLED,
                                  syncer::StringOrdinal(),
                                  "Param");
  }

  void Verify() override {
    EXPECT_TRUE(prefs()->IsExtensionDisabled(extension_->id()));
    EXPECT_EQ(std::string("Param"), prefs()->GetInstallParam(extension_->id()));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionPrefsOnExtensionInstalled,
       ExtensionPrefsOnExtensionInstalled) {}

class ExtensionPrefsAppDraggedByUser : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension_ = prefs_.AddExtension("on_extension_installed");
    EXPECT_FALSE(prefs()->WasAppDraggedByUser(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(),
                                  Extension::ENABLED,
                                  syncer::StringOrdinal(),
                                  std::string());
  }

  void Verify() override {
    // Set the flag and see if it persisted.
    prefs()->SetAppDraggedByUser(extension_->id());
    EXPECT_TRUE(prefs()->WasAppDraggedByUser(extension_->id()));

    // Make sure it doesn't change on consecutive calls.
    prefs()->SetAppDraggedByUser(extension_->id());
    EXPECT_TRUE(prefs()->WasAppDraggedByUser(extension_->id()));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionPrefsAppDraggedByUser, ExtensionPrefsAppDraggedByUser) {}

class ExtensionPrefsFlags : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    {
      base::DictionaryValue dictionary;
      dictionary.SetString(manifest_keys::kName, "from_webstore");
      dictionary.SetString(manifest_keys::kVersion, "0.1");
      dictionary.SetInteger(manifest_keys::kManifestVersion, 2);
      webstore_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary, Manifest::INTERNAL, Extension::FROM_WEBSTORE);
    }

    {
      base::DictionaryValue dictionary;
      dictionary.SetString(manifest_keys::kName, "from_bookmark");
      dictionary.SetString(manifest_keys::kVersion, "0.1");
      dictionary.SetInteger(manifest_keys::kManifestVersion, 2);
      bookmark_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary, Manifest::INTERNAL, Extension::FROM_BOOKMARK);
    }

    {
      base::DictionaryValue dictionary;
      dictionary.SetString(manifest_keys::kName, "was_installed_by_default");
      dictionary.SetString(manifest_keys::kVersion, "0.1");
      dictionary.SetInteger(manifest_keys::kManifestVersion, 2);
      default_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary,
          Manifest::INTERNAL,
          Extension::WAS_INSTALLED_BY_DEFAULT);
    }

    {
      base::DictionaryValue dictionary;
      dictionary.SetString(manifest_keys::kName, "was_installed_by_oem");
      dictionary.SetString(manifest_keys::kVersion, "0.1");
      dictionary.SetInteger(manifest_keys::kManifestVersion, 2);
      oem_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary, Manifest::INTERNAL, Extension::WAS_INSTALLED_BY_OEM);
    }
  }

  void Verify() override {
    EXPECT_TRUE(prefs()->IsFromWebStore(webstore_extension_->id()));
    EXPECT_FALSE(prefs()->IsFromBookmark(webstore_extension_->id()));

    EXPECT_TRUE(prefs()->IsFromBookmark(bookmark_extension_->id()));
    EXPECT_FALSE(prefs()->IsFromWebStore(bookmark_extension_->id()));

    EXPECT_TRUE(prefs()->WasInstalledByDefault(default_extension_->id()));
    EXPECT_TRUE(prefs()->WasInstalledByOem(oem_extension_->id()));
  }

 private:
  scoped_refptr<Extension> webstore_extension_;
  scoped_refptr<Extension> bookmark_extension_;
  scoped_refptr<Extension> default_extension_;
  scoped_refptr<Extension> oem_extension_;
};
TEST_F(ExtensionPrefsFlags, ExtensionPrefsFlags) {}

PrefsPrepopulatedTestBase::PrefsPrepopulatedTestBase()
    : ExtensionPrefsTest() {
  base::DictionaryValue simple_dict;
  std::string error;

  simple_dict.SetString(manifest_keys::kVersion, "1.0.0.0");
  simple_dict.SetInteger(manifest_keys::kManifestVersion, 2);
  simple_dict.SetString(manifest_keys::kName, "unused");

  extension1_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext1_"),
      Manifest::EXTERNAL_PREF,
      simple_dict,
      Extension::NO_FLAGS,
      &error);
  extension2_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext2_"),
      Manifest::EXTERNAL_PREF,
      simple_dict,
      Extension::NO_FLAGS,
      &error);
  extension3_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext3_"),
      Manifest::EXTERNAL_PREF,
      simple_dict,
      Extension::NO_FLAGS,
      &error);
  extension4_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext4_"),
      Manifest::EXTERNAL_PREF,
      simple_dict,
      Extension::NO_FLAGS,
      &error);

  internal_extension_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("internal extension"), Manifest::INTERNAL,
      simple_dict, Extension::NO_FLAGS, &error);

  for (size_t i = 0; i < kNumInstalledExtensions; ++i)
    installed_[i] = false;
}

PrefsPrepopulatedTestBase::~PrefsPrepopulatedTestBase() {
}

// Tests that blacklist state can be queried.
class ExtensionPrefsBlacklistedExtensions : public ExtensionPrefsTest {
 public:
  ~ExtensionPrefsBlacklistedExtensions() override {}

  void Initialize() override {
    extension_a_ = prefs_.AddExtension("a");
    extension_b_ = prefs_.AddExtension("b");
    extension_c_ = prefs_.AddExtension("c");
  }

  void Verify() override {
    {
      ExtensionIdSet ids;
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_a_->id(), true);
    {
      ExtensionIdSet ids;
      ids.insert(extension_a_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_b_->id(), true);
    prefs()->SetExtensionBlacklisted(extension_c_->id(), true);
    {
      ExtensionIdSet ids;
      ids.insert(extension_a_->id());
      ids.insert(extension_b_->id());
      ids.insert(extension_c_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_a_->id(), false);
    {
      ExtensionIdSet ids;
      ids.insert(extension_b_->id());
      ids.insert(extension_c_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_b_->id(), false);
    prefs()->SetExtensionBlacklisted(extension_c_->id(), false);
    {
      ExtensionIdSet ids;
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }

    // The interesting part: make sure that we're cleaning up after ourselves
    // when we're storing *just* the fact that the extension is blacklisted.
    std::string arbitrary_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    prefs()->SetExtensionBlacklisted(arbitrary_id, true);
    prefs()->SetExtensionBlacklisted(extension_a_->id(), true);

    // (And make sure that the acknowledged bit is also cleared).
    prefs()->AcknowledgeBlacklistedExtension(arbitrary_id);

    EXPECT_TRUE(prefs()->GetExtensionPref(arbitrary_id));
    {
      ExtensionIdSet ids;
      ids.insert(arbitrary_id);
      ids.insert(extension_a_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(arbitrary_id, false);
    prefs()->SetExtensionBlacklisted(extension_a_->id(), false);
    EXPECT_FALSE(prefs()->GetExtensionPref(arbitrary_id));
    {
      ExtensionIdSet ids;
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
  }

 private:
  scoped_refptr<const Extension> extension_a_;
  scoped_refptr<const Extension> extension_b_;
  scoped_refptr<const Extension> extension_c_;
};
TEST_F(ExtensionPrefsBlacklistedExtensions,
       ExtensionPrefsBlacklistedExtensions) {}

// Tests the blacklist state. Old "blacklist" preference should take precedence
// over new "blacklist_state".
class ExtensionPrefsBlacklistState : public ExtensionPrefsTest {
 public:
  ~ExtensionPrefsBlacklistState() override {}

  void Initialize() override { extension_a_ = prefs_.AddExtension("a"); }

  void Verify() override {
    ExtensionIdSet empty_ids;
    EXPECT_EQ(empty_ids, prefs()->GetBlacklistedExtensions());

    prefs()->SetExtensionBlacklistState(extension_a_->id(),
                                        BLACKLISTED_MALWARE);
    EXPECT_EQ(BLACKLISTED_MALWARE,
              prefs()->GetExtensionBlacklistState(extension_a_->id()));

    prefs()->SetExtensionBlacklistState(extension_a_->id(),
                                        BLACKLISTED_POTENTIALLY_UNWANTED);
    EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED,
              prefs()->GetExtensionBlacklistState(extension_a_->id()));
    EXPECT_FALSE(prefs()->IsExtensionBlacklisted(extension_a_->id()));
    EXPECT_EQ(empty_ids, prefs()->GetBlacklistedExtensions());

    prefs()->SetExtensionBlacklistState(extension_a_->id(),
                                        BLACKLISTED_MALWARE);
    EXPECT_TRUE(prefs()->IsExtensionBlacklisted(extension_a_->id()));
    EXPECT_EQ(BLACKLISTED_MALWARE,
              prefs()->GetExtensionBlacklistState(extension_a_->id()));
    EXPECT_EQ(1u, prefs()->GetBlacklistedExtensions().size());

    prefs()->SetExtensionBlacklistState(extension_a_->id(),
                                        NOT_BLACKLISTED);
    EXPECT_EQ(NOT_BLACKLISTED,
              prefs()->GetExtensionBlacklistState(extension_a_->id()));
    EXPECT_FALSE(prefs()->IsExtensionBlacklisted(extension_a_->id()));
    EXPECT_EQ(empty_ids, prefs()->GetBlacklistedExtensions());
  }

 private:
  scoped_refptr<const Extension> extension_a_;
};
TEST_F(ExtensionPrefsBlacklistState, ExtensionPrefsBlacklistState) {}

// Tests clearing the last launched preference.
class ExtensionPrefsClearLastLaunched : public ExtensionPrefsTest {
 public:
  ~ExtensionPrefsClearLastLaunched() override {}

  void Initialize() override {
    extension_a_ = prefs_.AddExtension("a");
    extension_b_ = prefs_.AddExtension("b");
  }

  void Verify() override {
    // Set last launched times for each extension.
    prefs()->SetLastLaunchTime(extension_a_->id(), base::Time::Now());
    prefs()->SetLastLaunchTime(extension_b_->id(), base::Time::Now());

    // Also set some other preference for one of the extensions.
    prefs()->SetAllowFileAccess(extension_a_->id(), true);

    // Now clear the launch times.
    prefs()->ClearLastLaunchTimes();

    // All launch times should be gone.
    EXPECT_EQ(base::Time(), prefs()->GetLastLaunchTime(extension_a_->id()));
    EXPECT_EQ(base::Time(), prefs()->GetLastLaunchTime(extension_b_->id()));

    // Other preferences should be untouched.
    EXPECT_TRUE(prefs()->AllowFileAccess(extension_a_->id()));
  }

 private:
  scoped_refptr<const Extension> extension_a_;
  scoped_refptr<const Extension> extension_b_;
};
TEST_F(ExtensionPrefsClearLastLaunched, ExtensionPrefsClearLastLaunched) {}

class ExtensionPrefsComponentExtension : public ExtensionPrefsTest {
 public:
  ~ExtensionPrefsComponentExtension() override {}
  void Initialize() override {
    // Adding a component extension.
    component_extension_ =
        ExtensionBuilder("a")
            .SetLocation(Manifest::COMPONENT)
            .SetPath(prefs_.extensions_dir().AppendASCII("a"))
            .Build();
    prefs_.AddExtension(component_extension_.get());

    // Adding a non component extension.
    no_component_extension_ =
        ExtensionBuilder("b")
            .SetLocation(Manifest::INTERNAL)
            .SetPath(prefs_.extensions_dir().AppendASCII("b"))
            .Build();
    prefs_.AddExtension(no_component_extension_.get());

    APIPermissionSet api_perms;
    api_perms.insert(APIPermission::kTab);
    api_perms.insert(APIPermission::kBookmark);
    api_perms.insert(APIPermission::kHistory);

    ManifestPermissionSet empty_manifest_permissions;

    URLPatternSet ehosts, shosts;
    AddPattern(&shosts, "chrome://print/*");

    active_perms_.reset(new PermissionSet(api_perms, empty_manifest_permissions,
                                          ehosts, shosts));
    // Set the active permissions.
    prefs()->SetActivePermissions(component_extension_->id(), *active_perms_);
    prefs()->SetActivePermissions(no_component_extension_->id(),
                                  *active_perms_);
  }

  void Verify() override {
    // Component extension can access chrome://print/*.
    std::unique_ptr<const PermissionSet> component_permissions =
        prefs()->GetActivePermissions(component_extension_->id());
    EXPECT_EQ(1u, component_permissions->scriptable_hosts().size());

    // Non Component extension can not access chrome://print/*.
    std::unique_ptr<const PermissionSet> no_component_permissions =
        prefs()->GetActivePermissions(no_component_extension_->id());
    EXPECT_EQ(0u, no_component_permissions->scriptable_hosts().size());

    // |URLPattern::SCHEME_CHROMEUI| scheme will be added in valid_schemes for
    // component extensions.
    URLPatternSet scriptable_hosts;
    std::string pref_key = "active_permissions.scriptable_host";
    int valid_schemes = URLPattern::SCHEME_ALL & ~URLPattern::SCHEME_CHROMEUI;

    EXPECT_TRUE(prefs()->ReadPrefAsURLPatternSet(component_extension_->id(),
                                                 pref_key, &scriptable_hosts,
                                                 valid_schemes));

    EXPECT_FALSE(prefs()->ReadPrefAsURLPatternSet(no_component_extension_->id(),
                                                  pref_key, &scriptable_hosts,
                                                  valid_schemes));

    // Both extensions should be registered with the ExtensionPrefValueMap.
    // See https://crbug.com/454513.
    EXPECT_TRUE(prefs_.extension_pref_value_map()->CanExtensionControlPref(
        component_extension_->id(), "a_pref", false));
    EXPECT_TRUE(prefs_.extension_pref_value_map()->CanExtensionControlPref(
        no_component_extension_->id(), "a_pref", false));
  }

 private:
  std::unique_ptr<const PermissionSet> active_perms_;
  scoped_refptr<const Extension> component_extension_;
  scoped_refptr<const Extension> no_component_extension_;
};
TEST_F(ExtensionPrefsComponentExtension, ExtensionPrefsComponentExtension) {
}

// Tests reading and writing runtime granted permissions.
class ExtensionPrefsRuntimeGrantedPermissions : public ExtensionPrefsTest {
 public:
  ExtensionPrefsRuntimeGrantedPermissions() = default;
  ~ExtensionPrefsRuntimeGrantedPermissions() override {}

  void Initialize() override {
    extension_a_ = prefs_.AddExtension("a");
    extension_b_ = prefs_.AddExtension("b");

    // By default, runtime-granted permissions are empty.
    EXPECT_TRUE(
        prefs()->GetRuntimeGrantedPermissions(extension_a_->id())->IsEmpty());
    EXPECT_TRUE(
        prefs()->GetRuntimeGrantedPermissions(extension_b_->id())->IsEmpty());

    URLPattern example_com(URLPattern::SCHEME_ALL, "https://example.com/*");
    URLPattern chromium_org(URLPattern::SCHEME_ALL, "https://chromium.org/*");

    {
      // Add two hosts to the runtime granted permissions. Verify they were
      // correctly added.
      URLPatternSet added_urls({example_com, chromium_org});
      PermissionSet added_permissions(APIPermissionSet(),
                                      ManifestPermissionSet(), added_urls,
                                      URLPatternSet());
      prefs()->AddRuntimeGrantedPermissions(extension_a_->id(),
                                            added_permissions);

      std::unique_ptr<const PermissionSet> retrieved_permissions =
          prefs()->GetRuntimeGrantedPermissions(extension_a_->id());
      ASSERT_TRUE(retrieved_permissions);
      EXPECT_EQ(added_permissions, *retrieved_permissions);
    }

    {
      // Remove one of the hosts. The only remaining host should be
      // example.com
      URLPatternSet removed_urls({chromium_org});
      PermissionSet removed_permissions(APIPermissionSet(),
                                        ManifestPermissionSet(), removed_urls,
                                        URLPatternSet());
      prefs()->RemoveRuntimeGrantedPermissions(extension_a_->id(),
                                               removed_permissions);

      URLPatternSet remaining_urls({example_com});
      PermissionSet remaining_permissions(APIPermissionSet(),
                                          ManifestPermissionSet(),
                                          remaining_urls, URLPatternSet());
      std::unique_ptr<const PermissionSet> retrieved_permissions =
          prefs()->GetRuntimeGrantedPermissions(extension_a_->id());
      ASSERT_TRUE(retrieved_permissions);
      EXPECT_EQ(remaining_permissions, *retrieved_permissions);
    }

    // The second extension should still have no runtime-granted permissions.
    EXPECT_TRUE(
        prefs()->GetRuntimeGrantedPermissions(extension_b_->id())->IsEmpty());
  }

  void Verify() override {
    {
      // The first extension should still have example.com as the granted
      // permission.
      URLPattern example_com(URLPattern::SCHEME_ALL, "https://example.com/*");
      URLPatternSet remaining_urls({example_com});
      PermissionSet remaining_permissions(APIPermissionSet(),
                                          ManifestPermissionSet(),
                                          remaining_urls, URLPatternSet());
      std::unique_ptr<const PermissionSet> retrieved_permissions =
          prefs()->GetRuntimeGrantedPermissions(extension_a_->id());
      ASSERT_TRUE(retrieved_permissions);
      EXPECT_EQ(remaining_permissions, *retrieved_permissions);
    }

    EXPECT_TRUE(
        prefs()->GetRuntimeGrantedPermissions(extension_b_->id())->IsEmpty());
  }

 private:
  scoped_refptr<const Extension> extension_a_;
  scoped_refptr<const Extension> extension_b_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefsRuntimeGrantedPermissions);
};
TEST_F(ExtensionPrefsRuntimeGrantedPermissions,
       ExtensionPrefsRuntimeGrantedPermissions) {}

}  // namespace extensions
