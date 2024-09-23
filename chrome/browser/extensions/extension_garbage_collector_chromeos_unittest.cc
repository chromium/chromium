// Copyright 2014 The Chromium Authors
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
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
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
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/install_flag.h"
#include "extensions/common/extension_builder.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

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
        std::make_unique<ash::FakeChromeUserManager>());

    GetFakeUserManager()->AddUser(user_manager::StubAccountId());
    GetFakeUserManager()->LoginUser(user_manager::StubAccountId());
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
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
    ScopedDictPrefUpdate shared_extensions(
        testing_local_state_.Get(),
        ExtensionAssetsManagerChromeOS::kSharedExtensions);

    base::Value::Dict* extension_info_weak = shared_extensions->EnsureDict(id);

    base::Value::Dict version_info;
    version_info.Set(ExtensionAssetsManagerChromeOS::kSharedExtensionPath,
                     path.value());

    base::Value::List users;
    for (const std::string& user :
         base::SplitString(users_string, ",", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      users.Append(user);
    }
    version_info.Set(ExtensionAssetsManagerChromeOS::kSharedExtensionUsers,
                     std::move(users));
    extension_info_weak->Set(version, std::move(version_info));
  }

  scoped_refptr<const Extension> CreateExtension(const std::string& id,
                                                 const std::string& version,
                                                 const base::FilePath& path) {
    return ExtensionBuilder("test")
        .SetVersion(version)
        .SetID(id)
        .SetPath(path)
        .SetLocation(mojom::ManifestLocation::kInternal)
        .Build();
  }

  ExtensionPrefs* GetExtensionPrefs() {
    return ExtensionPrefs::Get(profile_.get());
  }

  ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
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
      extension2.get(), Extension::ENABLED, kInstallFlagNone,
      ExtensionPrefs::DelayReason::kWaitForIdle, syncer::StringOrdinal(),
      std::string());
  EXPECT_TRUE(base::PathExists(path_id2_1));

  GarbageCollectExtensions();

  EXPECT_FALSE(base::PathExists(path_id1_1));
  EXPECT_FALSE(base::PathExists(path_id1_2));
  EXPECT_FALSE(base::PathExists(cache_dir().AppendASCII(kExtensionId1)));

  EXPECT_TRUE(base::PathExists(path_id2_1));

  const base::Value::Dict& shared_extensions =
      testing_local_state_.Get()->GetDict(
          ExtensionAssetsManagerChromeOS::kSharedExtensions);

  EXPECT_FALSE(shared_extensions.Find(kExtensionId1));
  EXPECT_TRUE(shared_extensions.Find(kExtensionId2));
}

}  // namespace extensions
