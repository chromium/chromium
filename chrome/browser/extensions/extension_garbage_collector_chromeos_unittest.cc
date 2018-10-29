// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector_chromeos.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/install_flag.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "ppapi/buildflags/buildflags.h"

namespace {
const char kExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
}  // namespace

namespace extensions {

class ExtensionGarbageCollectorChromeOSUnitTest
    : public ExtensionServiceTestBase {
 protected:
  const base::FilePath& cache_dir() { return cache_dir_.GetPath(); }

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->Init();
#endif
    InitializeGoodInstalledExtensionService();

    CHECK(cache_dir_.CreateUniqueTempDir());
    ExtensionAssetsManagerChromeOS::SetSharedInstallDirForTesting(cache_dir());
    ExtensionGarbageCollectorChromeOS::ClearGarbageCollectedForTesting();

    // Initialize the UserManager singleton to a fresh FakeChromeUserManager
    // instance.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());

    GetFakeUserManager()->AddUser(user_manager::StubAccountId());
    GetFakeUserManager()->LoginUser(user_manager::StubAccountId());
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        GetFakeUserManager()->GetActiveUser(), profile_.get());
  }

  void GarbageCollectExtensions() {
    ExtensionGarbageCollector::Get(profile_.get())
        ->GarbageCollectExtensionsForTest();
    // Wait for GarbageCollectExtensions task to complete.
    content::RunAllTasksUntilIdle();
  }

  base::FilePath CreateSharedExtensionDir(const std::string& id,
                                          const std::string& version,
                                          const base::FilePath& shared_dir) {
    base::FilePath path = shared_dir.AppendASCII(id).AppendASCII(version);
    CreateDirectory(path);
    return path;
  }

  void CreateSharedExtensionPrefs(const std::string& id,
                                  const std::string& version,
                                  const std::string& users_string,
                                  const base::FilePath& path) {
    DictionaryPrefUpdate shared_extensions(testing_local_state_.Get(),
        ExtensionAssetsManagerChromeOS::kSharedExtensions);

    base::DictionaryValue* extension_info_weak = NULL;
    if (!shared_extensions->GetDictionary(id, &extension_info_weak)) {
      auto extension_info = std::make_unique<base::DictionaryValue>();
      extension_info_weak = extension_info.get();
      shared_extensions->Set(id, std::move(extension_info));
    }

    auto version_info = std::make_unique<base::DictionaryValue>();
    version_info->SetString(
        ExtensionAssetsManagerChromeOS::kSharedExtensionPath, path.value());

    auto users = std::make_unique<base::ListValue>();
    for (const std::string& user :
         base::SplitString(users_string, ",", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      users->AppendString(user);
    }
    version_info->Set(ExtensionAssetsManagerChromeOS::kSharedExtensionUsers,
                      std::move(users));
    extension_info_weak->SetWithoutPathExpansion(version,
                                                 std::move(version_info));
  }

  scoped_refptr<const Extension> CreateExtension(const std::string& id,
                                                 const std::string& version,
                                                 const base::FilePath& path) {
    return ExtensionBuilder("test")
        .SetVersion(version)
        .SetID(id)
        .SetPath(path)
        .SetLocation(Manifest::INTERNAL)
        .Build();
  }

  ExtensionPrefs* GetExtensionPrefs() {
    return ExtensionPrefs::Get(profile_.get());
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir cache_dir_;
};

// Test shared extensions clean up.
TEST_F(ExtensionGarbageCollectorChromeOSUnitTest, SharedExtensions) {
  // Version for non-existing user.
  base::FilePath path_id1_1 = CreateSharedExtensionDir(
      kExtensionId1, "1.0", cache_dir());
  CreateSharedExtensionPrefs(kExtensionId1, "1.0", "test@test.com", path_id1_1);
  EXPECT_TRUE(base::PathExists(path_id1_1));

  // Version for current user but the extension is not installed.
  base::FilePath path_id1_2 = CreateSharedExtensionDir(
      kExtensionId1, "2.0", cache_dir());
  CreateSharedExtensionPrefs(kExtensionId1, "2.0",
                             user_manager::StubAccountId().GetUserEmail(),
                             path_id1_2);
  EXPECT_TRUE(base::PathExists(path_id1_2));

  // Version for current user that delayed install.
  base::FilePath path_id2_1 = CreateSharedExtensionDir(
      kExtensionId2, "1.0", cache_dir());
  CreateSharedExtensionPrefs(kExtensionId2, "1.0",
                             user_manager::StubAccountId().GetUserEmail(),
                             path_id2_1);
  scoped_refptr<const Extension> extension2 =
      CreateExtension(kExtensionId2, "1.0", path_id2_1);
  GetExtensionPrefs()->SetDelayedInstallInfo(
      extension2.get(),
      Extension::ENABLED,
      kInstallFlagNone,
      ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE,
      syncer::StringOrdinal(),
      std::string());
  EXPECT_TRUE(base::PathExists(path_id2_1));

  GarbageCollectExtensions();

  EXPECT_FALSE(base::PathExists(path_id1_1));
  EXPECT_FALSE(base::PathExists(path_id1_2));
  EXPECT_FALSE(base::PathExists(cache_dir().AppendASCII(kExtensionId1)));

  EXPECT_TRUE(base::PathExists(path_id2_1));

  const base::DictionaryValue* shared_extensions = testing_local_state_.Get()->
      GetDictionary(ExtensionAssetsManagerChromeOS::kSharedExtensions);
  ASSERT_TRUE(shared_extensions);

  EXPECT_FALSE(shared_extensions->HasKey(kExtensionId1));
  EXPECT_TRUE(shared_extensions->HasKey(kExtensionId2));
}

}  // namespace extensions
