// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/registry.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
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
// a base::DictionaryValue, so it has to be stored without path expansion.
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

  base::DictionaryValue extensions;
  auto file_system = std::make_unique<base::DictionaryValue>();
  file_system->SetKey(kPrefKeyFileSystemId, base::Value(kFileSystemId));
  file_system->SetKey(kPrefKeyDisplayName, base::Value(kDisplayName));
  file_system->SetKey(kPrefKeyWritable, base::Value(writable));
  file_system->SetKey(kPrefKeySupportsNotifyTag,
                      base::Value(supports_notify_tag));
  file_system->SetKey(kPrefKeyOpenedFilesLimit,
                      base::Value(opened_files_limit));

  // Remember watchers.
  auto watcher_value = std::make_unique<base::DictionaryValue>();
  watcher_value->SetKey(kPrefKeyWatcherEntryPath,
                        base::Value(watcher.entry_path.value()));
  watcher_value->SetKey(kPrefKeyWatcherRecursive,
                        base::Value(watcher.recursive));
  watcher_value->SetKey(kPrefKeyWatcherLastTag, base::Value(watcher.last_tag));
  auto persistent_origins_value = std::make_unique<base::ListValue>();
  for (const auto& subscriber_it : watcher.subscribers) {
    if (subscriber_it.second.persistent)
      persistent_origins_value->AppendString(subscriber_it.first.spec());
  }

  watcher_value->SetWithoutPathExpansion(kPrefKeyWatcherPersistentOrigins,
                                         std::move(persistent_origins_value));
  auto watchers = std::make_unique<base::DictionaryValue>();
  watchers->SetWithoutPathExpansion(watcher.entry_path.value(),
                                    std::move(watcher_value));
  file_system->SetWithoutPathExpansion(kPrefKeyWatchers, std::move(watchers));
  auto file_systems = std::make_unique<base::DictionaryValue>();
  file_systems->SetWithoutPathExpansion(kFileSystemId, std::move(file_system));
  extensions.SetWithoutPathExpansion(kProviderId.ToString(),
                                     std::move(file_systems));
  pref_service->Set(prefs::kFileSystemProviderMounted, extensions);
}

}  // namespace

class FileSystemProviderRegistryTest : public testing::Test {
 protected:
  FileSystemProviderRegistryTest() : profile_(NULL) {}

  ~FileSystemProviderRegistryTest() override {}

  void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    registry_.reset(new Registry(profile_));
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
  TestingProfile* profile_;
  std::unique_ptr<RegistryInterface> registry_;
  Watcher fake_watcher_;
};

TEST_F(FileSystemProviderRegistryTest, RestoreFileSystems) {
  // Create a fake entry in the preferences.
  RememberFakeFileSystem(profile_, kProviderId, kFileSystemId, kDisplayName,
                         true /* writable */, true /* supports_notify_tag */,
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
      false /* configurable */, true /* watchable */, extensions::SOURCE_FILE,
      IconSet());

  Watchers watchers;
  watchers[WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive)] =
      fake_watcher_;

  registry_->RememberFileSystem(file_system_info, watchers);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(
      kProviderId.ToString(), &file_systems));
  EXPECT_EQ(1u, file_systems->size());

  const base::Value* file_system_value = NULL;
  const base::DictionaryValue* file_system = NULL;
  ASSERT_TRUE(
      file_systems->GetWithoutPathExpansion(kFileSystemId, &file_system_value));
  ASSERT_TRUE(file_system_value->GetAsDictionary(&file_system));

  std::string file_system_id;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                                         &file_system_id));
  EXPECT_EQ(kFileSystemId, file_system_id);

  std::string display_name;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyDisplayName,
                                                         &display_name));
  EXPECT_EQ(kDisplayName, display_name);

  bool writable = false;
  EXPECT_TRUE(
      file_system->GetBooleanWithoutPathExpansion(kPrefKeyWritable, &writable));
  EXPECT_TRUE(writable);

  bool supports_notify_tag = false;
  EXPECT_TRUE(file_system->GetBooleanWithoutPathExpansion(
      kPrefKeySupportsNotifyTag, &supports_notify_tag));
  EXPECT_TRUE(supports_notify_tag);

  int opened_files_limit = 0;
  EXPECT_TRUE(file_system->GetIntegerWithoutPathExpansion(
      kPrefKeyOpenedFilesLimit, &opened_files_limit));
  EXPECT_EQ(kOpenedFilesLimit, opened_files_limit);

  const base::DictionaryValue* watchers_value = NULL;
  ASSERT_TRUE(file_system->GetDictionaryWithoutPathExpansion(kPrefKeyWatchers,
                                                             &watchers_value));

  const base::DictionaryValue* watcher = NULL;
  ASSERT_TRUE(watchers_value->GetDictionaryWithoutPathExpansion(
      fake_watcher_.entry_path.value(), &watcher));

  std::string entry_path;
  EXPECT_TRUE(watcher->GetStringWithoutPathExpansion(kPrefKeyWatcherEntryPath,
                                                     &entry_path));
  EXPECT_EQ(fake_watcher_.entry_path.value(), entry_path);

  bool recursive = false;
  EXPECT_TRUE(watcher->GetBooleanWithoutPathExpansion(kPrefKeyWatcherRecursive,
                                                      &recursive));
  EXPECT_EQ(fake_watcher_.recursive, recursive);

  std::string last_tag;
  EXPECT_TRUE(watcher->GetStringWithoutPathExpansion(kPrefKeyWatcherLastTag,
                                                     &last_tag));
  EXPECT_EQ(fake_watcher_.last_tag, last_tag);

  const base::ListValue* persistent_origins = NULL;
  ASSERT_TRUE(watcher->GetListWithoutPathExpansion(
      kPrefKeyWatcherPersistentOrigins, &persistent_origins));
  ASSERT_GT(fake_watcher_.subscribers.size(), persistent_origins->GetSize());
  ASSERT_EQ(1u, persistent_origins->GetSize());
  std::string persistent_origin;
  EXPECT_TRUE(persistent_origins->GetString(0, &persistent_origin));
  const auto& fake_subscriber_it =
      fake_watcher_.subscribers.find(GURL(persistent_origin));
  ASSERT_NE(fake_watcher_.subscribers.end(), fake_subscriber_it);
  EXPECT_TRUE(fake_subscriber_it->second.persistent);
}

TEST_F(FileSystemProviderRegistryTest, ForgetFileSystem) {
  // Create a fake file systems in the preferences.
  RememberFakeFileSystem(profile_, kProviderId, kFileSystemId, kDisplayName,
                         true /* writable */, true /* supports_notify_tag */,
                         kOpenedFilesLimit, fake_watcher_);

  registry_->ForgetFileSystem(kProviderId, kFileSystemId);

  sync_preferences::TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  EXPECT_FALSE(extensions->GetDictionaryWithoutPathExpansion(
      kProviderId.GetExtensionId(), &file_systems));
}

TEST_F(FileSystemProviderRegistryTest, UpdateWatcherTag) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;

  ProvidedFileSystemInfo file_system_info(
      kProviderId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")),
      false /* configurable */, true /* watchable */, extensions::SOURCE_FILE,
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

  const base::DictionaryValue* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(
      kProviderId.ToString(), &file_systems));
  EXPECT_EQ(1u, file_systems->size());

  const base::Value* file_system_value = NULL;
  const base::DictionaryValue* file_system = NULL;
  ASSERT_TRUE(
      file_systems->GetWithoutPathExpansion(kFileSystemId, &file_system_value));
  ASSERT_TRUE(file_system_value->GetAsDictionary(&file_system));

  const base::DictionaryValue* watchers_value = NULL;
  ASSERT_TRUE(file_system->GetDictionaryWithoutPathExpansion(kPrefKeyWatchers,
                                                             &watchers_value));

  const base::DictionaryValue* watcher = NULL;
  ASSERT_TRUE(watchers_value->GetDictionaryWithoutPathExpansion(
      fake_watcher_.entry_path.value(), &watcher));

  std::string last_tag;
  EXPECT_TRUE(watcher->GetStringWithoutPathExpansion(kPrefKeyWatcherLastTag,
                                                     &last_tag));
  EXPECT_EQ(fake_watcher_.last_tag, last_tag);
}

}  // namespace file_system_provider
}  // namespace chromeos
