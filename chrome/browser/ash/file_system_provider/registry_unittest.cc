// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/registry.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

const char kTemporaryOrigin[] =
    "chrome-extension://abcabcabcabcabcabcabcabcabcabcabcabca/";
const char kPersistentOrigin[] =
    "chrome-extension://efgefgefgefgefgefgefgefgefgefgefgefge/";
const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kDisplayName[] = "Camera Pictures";
const ProviderId kProviderId = ProviderId::CreateFromExtensionId(kExtensionId);

// The dot in the file system ID is there in order to check that saving to
// preferences works correctly. File System ID is used as a key in
// a base::Value, so it has to be stored without path expansion.
const char kFileSystemId[] = "camera/pictures/id .!@#$%^&*()_+";

const int kOpenedFilesLimit = 5;

// Stores a provided file system information in preferences together with a
// fake watcher.
void RememberFakeFileSystem(TestingProfile* profile,
                            const ProviderId& provider_id,
                            const std::string& file_system_id,
                            const std::string& display_name,
                            bool writable,
                            bool supports_notify_tag,
                            int opened_files_limit,
                            const Watcher& watcher) {
  // Warning. Updating this code means that backward compatibility may be
  // broken, what is unexpected and should be avoided.
  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  base::Value::Dict extensions;
  base::Value::Dict file_system;
  file_system.Set(kPrefKeyFileSystemId, kFileSystemId);
  file_system.Set(kPrefKeyDisplayName, kDisplayName);
  file_system.Set(kPrefKeyWritable, writable);
  file_system.Set(kPrefKeySupportsNotifyTag, supports_notify_tag);
  file_system.Set(kPrefKeyOpenedFilesLimit, opened_files_limit);

  // Remember watchers.
  base::Value::Dict watcher_value;
  watcher_value.Set(kPrefKeyWatcherEntryPath, watcher.entry_path.value());
  watcher_value.Set(kPrefKeyWatcherRecursive, watcher.recursive);
  watcher_value.Set(kPrefKeyWatcherLastTag, watcher.last_tag);
  base::Value::List persistent_origins_value;
  for (const auto& subscriber_it : watcher.subscribers) {
    if (subscriber_it.second.persistent)
      persistent_origins_value.Append(subscriber_it.first.spec());
  }

  watcher_value.Set(kPrefKeyWatcherPersistentOrigins,
                    std::move(persistent_origins_value));
  base::Value::Dict watchers;
  watchers.Set(watcher.entry_path.value(), std::move(watcher_value));
  file_system.Set(kPrefKeyWatchers, std::move(watchers));
  base::Value::Dict file_systems;
  file_systems.Set(kFileSystemId, std::move(file_system));
  extensions.Set(kProviderId.ToString(), std::move(file_systems));
  pref_service->SetDict(prefs::kFileSystemProviderMounted,
                        std::move(extensions));
}

}  // namespace

class FileSystemProviderRegistryTest : public testing::Test {
 protected:
  FileSystemProviderRegistryTest() : profile_(nullptr) {}

  ~FileSystemProviderRegistryTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    registry_ = std::make_unique<Registry>(profile_);
    fake_watcher_.entry_path = base::FilePath(FILE_PATH_LITERAL("/a/b/c"));
    fake_watcher_.recursive = true;
    fake_watcher_.subscribers[GURL(kTemporaryOrigin)].origin =
        GURL(kTemporaryOrigin);
    fake_watcher_.subscribers[GURL(kTemporaryOrigin)].persistent = false;
    fake_watcher_.subscribers[GURL(kPersistentOrigin)].origin =
        GURL(kPersistentOrigin);
    fake_watcher_.subscribers[GURL(kPersistentOrigin)].persistent = true;
    fake_watcher_.last_tag = "hello-world";
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<RegistryInterface> registry_;
  Watcher fake_watcher_;
};

TEST_F(FileSystemProviderRegistryTest, RestoreFileSystems) {
  // Create a fake entry in the preferences.
  RememberFakeFileSystem(profile_, kProviderId, kFileSystemId, kDisplayName,
                         /*writable=*/true, /*supports_notify_tag=*/true,
                         kOpenedFilesLimit, fake_watcher_);

  std::unique_ptr<RegistryInterface::RestoredFileSystems>
      restored_file_systems = registry_->RestoreFileSystems(kProviderId);

  ASSERT_EQ(1u, restored_file_systems->size());
  const RegistryInterface::RestoredFileSystem& restored_file_system =
      restored_file_systems->at(0);
  EXPECT_EQ(kProviderId, restored_file_system.provider_id);
  EXPECT_EQ(kFileSystemId, restored_file_system.options.file_system_id);
  EXPECT_EQ(kDisplayName, restored_file_system.options.display_name);
  EXPECT_TRUE(restored_file_system.options.writable);
  EXPECT_TRUE(restored_file_system.options.supports_notify_tag);
  EXPECT_EQ(kOpenedFilesLimit, restored_file_system.options.opened_files_limit);

  ASSERT_EQ(1u, restored_file_system.watchers.size());
  const auto& restored_watcher_it = restored_file_system.watchers.find(
      WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive));
  ASSERT_NE(restored_file_system.watchers.end(), restored_watcher_it);

  EXPECT_EQ(fake_watcher_.entry_path, restored_watcher_it->second.entry_path);
  EXPECT_EQ(fake_watcher_.recursive, restored_watcher_it->second.recursive);
  EXPECT_EQ(fake_watcher_.last_tag, restored_watcher_it->second.last_tag);
}

TEST_F(FileSystemProviderRegistryTest, RememberFileSystem) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;
  options.opened_files_limit = kOpenedFilesLimit;

  ProvidedFileSystemInfo file_system_info(
      kProviderId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")),
      /*configurable=*/false, /*watchable=*/true, extensions::SOURCE_FILE,
      IconSet());

  Watchers watchers;
  watchers[WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive)] =
      fake_watcher_;

  registry_->RememberFileSystem(file_system_info, watchers);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::Value::Dict& extensions =
      pref_service->GetDict(prefs::kFileSystemProviderMounted);

  const base::Value::Dict* file_systems =
      extensions.FindDict(kProviderId.ToString());
  ASSERT_TRUE(file_systems);
  EXPECT_EQ(1u, file_systems->size());

  const base::Value::Dict* file_system = file_systems->FindDict(kFileSystemId);
  ASSERT_TRUE(file_system);

  const std::string* file_system_id =
      file_system->FindString(kPrefKeyFileSystemId);
  EXPECT_TRUE(file_system_id);
  EXPECT_EQ(kFileSystemId, *file_system_id);

  const std::string* display_name =
      file_system->FindString(kPrefKeyDisplayName);
  EXPECT_TRUE(display_name);
  EXPECT_EQ(kDisplayName, *display_name);

  std::optional<bool> writable = file_system->FindBool(kPrefKeyWritable);
  EXPECT_TRUE(writable.has_value());
  EXPECT_TRUE(writable.value());

  std::optional<bool> supports_notify_tag =
      file_system->FindBool(kPrefKeySupportsNotifyTag);
  EXPECT_TRUE(supports_notify_tag.has_value());
  EXPECT_TRUE(supports_notify_tag.value());

  std::optional<int> opened_files_limit =
      file_system->FindInt(kPrefKeyOpenedFilesLimit);
  EXPECT_TRUE(opened_files_limit.has_value());
  EXPECT_EQ(kOpenedFilesLimit, opened_files_limit.value());

  const base::Value::Dict* watchers_dict =
      file_system->FindDict(kPrefKeyWatchers);
  ASSERT_TRUE(watchers_dict);

  const base::Value::Dict* watcher =
      watchers_dict->FindDict(fake_watcher_.entry_path.value());
  ASSERT_TRUE(watcher);

  const std::string* entry_path = watcher->FindString(kPrefKeyWatcherEntryPath);
  EXPECT_TRUE(entry_path);
  EXPECT_EQ(fake_watcher_.entry_path.value(), *entry_path);

  std::optional<bool> recursive = watcher->FindBool(kPrefKeyWatcherRecursive);
  EXPECT_TRUE(recursive.has_value());
  EXPECT_EQ(fake_watcher_.recursive, recursive.value());

  const std::string* last_tag = watcher->FindString(kPrefKeyWatcherLastTag);
  EXPECT_TRUE(last_tag);
  EXPECT_EQ(fake_watcher_.last_tag, *last_tag);

  const base::Value::List* persistent_origins =
      watcher->FindList(kPrefKeyWatcherPersistentOrigins);
  ASSERT_TRUE(persistent_origins);
  ASSERT_GT(fake_watcher_.subscribers.size(), persistent_origins->size());
  ASSERT_EQ(1u, persistent_origins->size());
  const std::string* persistent_origin =
      persistent_origins->front().GetIfString();
  ASSERT_TRUE(persistent_origin);
  const auto& fake_subscriber_it =
      fake_watcher_.subscribers.find(GURL(*persistent_origin));
  ASSERT_NE(fake_watcher_.subscribers.end(), fake_subscriber_it);
  EXPECT_TRUE(fake_subscriber_it->second.persistent);
}

TEST_F(FileSystemProviderRegistryTest, ForgetFileSystem) {
  // Create a fake file systems in the preferences.
  RememberFakeFileSystem(profile_, kProviderId, kFileSystemId, kDisplayName,
                         /*writable=*/true, /*supports_notify_tag=*/true,
                         kOpenedFilesLimit, fake_watcher_);

  registry_->ForgetFileSystem(kProviderId, kFileSystemId);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::Value::Dict& extensions =
      pref_service->GetDict(prefs::kFileSystemProviderMounted);

  const base::Value::Dict* file_systems =
      extensions.FindDict(kProviderId.GetExtensionId());
  EXPECT_FALSE(file_systems);
}

TEST_F(FileSystemProviderRegistryTest, UpdateWatcherTag) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;

  ProvidedFileSystemInfo file_system_info(
      kProviderId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")),
      /*configurable=*/false, /*watchable=*/true, extensions::SOURCE_FILE,
      IconSet());

  Watchers watchers;
  watchers[WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive)] =
      fake_watcher_;

  registry_->RememberFileSystem(file_system_info, watchers);

  fake_watcher_.last_tag = "updated-tag";
  registry_->UpdateWatcherTag(file_system_info, fake_watcher_);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::Value::Dict& extensions =
      pref_service->GetDict(prefs::kFileSystemProviderMounted);

  const base::Value::Dict* file_systems =
      extensions.FindDict(kProviderId.ToString());
  ASSERT_TRUE(file_systems);
  EXPECT_EQ(1u, file_systems->size());

  const base::Value::Dict* file_system = file_systems->FindDict(kFileSystemId);
  ASSERT_TRUE(file_system);

  const base::Value::Dict* watchers_dict =
      file_system->FindDict(kPrefKeyWatchers);
  ASSERT_TRUE(watchers_dict);

  const base::Value::Dict* watcher =
      watchers_dict->FindDict(fake_watcher_.entry_path.value());
  ASSERT_TRUE(watcher);

  const std::string* last_tag = watcher->FindString(kPrefKeyWatcherLastTag);
  EXPECT_TRUE(last_tag);
  EXPECT_EQ(fake_watcher_.last_tag, *last_tag);
}

}  // namespace ash::file_system_provider
