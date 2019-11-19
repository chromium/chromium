// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kClientId[] = "client-id";

class MockSupervisedUserWhitelistInstaller
    : public component_updater::SupervisedUserWhitelistInstaller {
 public:
  MockSupervisedUserWhitelistInstaller() {}
  ~MockSupervisedUserWhitelistInstaller() override {}

  const std::set<std::string>& registered_whitelists() {
    return registered_whitelists_;
  }

  void NotifyWhitelistReady(const std::string& crx_id,
                            const base::string16& title,
                            const base::FilePath& large_icon_path,
                            const base::FilePath& whitelist_path) {
    for (const auto& callback : ready_callbacks_)
      callback.Run(crx_id, title, large_icon_path, whitelist_path);
  }

  // SupervisedUserWhitelistInstaller implementation:
  void RegisterComponents() override {}

  void Subscribe(const WhitelistReadyCallback& callback) override {
    ready_callbacks_.push_back(callback);
  }

  void RegisterWhitelist(const std::string& client_id,
                         const std::string& crx_id,
                         const std::string& name) override {
    EXPECT_EQ(kClientId, client_id);
    EXPECT_FALSE(WhitelistIsRegistered(crx_id)) << crx_id;
    registered_whitelists_.insert(crx_id);
  }

  void UnregisterWhitelist(const std::string& client_id,
                           const std::string& crx_id) override {
    EXPECT_EQ(kClientId, client_id);
    EXPECT_TRUE(WhitelistIsRegistered(crx_id)) << crx_id;
    registered_whitelists_.erase(crx_id);
  }

 private:
  bool WhitelistIsRegistered(const std::string& crx_id) {
    return registered_whitelists_.count(crx_id) > 0;
  }

  std::set<std::string> registered_whitelists_;
  std::vector<WhitelistReadyCallback> ready_callbacks_;
};

}  // namespace

class SupervisedUserWhitelistServiceTest : public testing::Test {
 public:
  SupervisedUserWhitelistServiceTest()
      : installer_(new MockSupervisedUserWhitelistInstaller),
        service_(new SupervisedUserWhitelistService(profile_.GetPrefs(),
                                                    installer_.get(),
                                                    kClientId)) {
    service_->AddSiteListsChangedCallback(
        base::Bind(&SupervisedUserWhitelistServiceTest::OnSiteListsChanged,
                   base::Unretained(this)));
  }

 protected:
  void PrepareInitialStateAndPreferences() {
    // Create two whitelists.
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                prefs::kSupervisedUserWhitelists);
    base::DictionaryValue* dict = update.Get();

    std::unique_ptr<base::DictionaryValue> whitelist_dict(
        new base::DictionaryValue);
    whitelist_dict->SetString("name", "Whitelist A");
    dict->Set("aaaa", std::move(whitelist_dict));

    whitelist_dict.reset(new base::DictionaryValue);
    whitelist_dict->SetString("name", "Whitelist B");
    dict->Set("bbbb", std::move(whitelist_dict));

    installer_->RegisterWhitelist(kClientId, "aaaa", "Whitelist A");
    installer_->RegisterWhitelist(kClientId, "bbbb", "Whitelist B");
  }

  void CheckFinalStateAndPreferences() {
    EXPECT_EQ(2u, installer_->registered_whitelists().size());
    EXPECT_EQ(1u, installer_->registered_whitelists().count("bbbb"));
    EXPECT_EQ(1u, installer_->registered_whitelists().count("cccc"));

    const base::DictionaryValue* dict =
        profile_.GetPrefs()->GetDictionary(prefs::kSupervisedUserWhitelists);
    EXPECT_EQ(2u, dict->size());
    const base::DictionaryValue* whitelist_dict = nullptr;
    ASSERT_TRUE(dict->GetDictionary("bbbb", &whitelist_dict));
    std::string name;
    ASSERT_TRUE(whitelist_dict->GetString("name", &name));
    EXPECT_EQ("Whitelist B New", name);
    ASSERT_TRUE(dict->GetDictionary("cccc", &whitelist_dict));
    ASSERT_TRUE(whitelist_dict->GetString("name", &name));
    EXPECT_EQ("Whitelist C", name);
  }

  const sync_pb::ManagedUserWhitelistSpecifics* FindWhitelist(
      const syncer::SyncDataList& data_list,
      const std::string& id) {
    for (const syncer::SyncData& data : data_list) {
      const sync_pb::ManagedUserWhitelistSpecifics& whitelist =
          data.GetSpecifics().managed_user_whitelist();
      if (whitelist.id() == id)
        return &whitelist;
    }
    return nullptr;
  }

  void OnSiteListsChanged(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
    site_lists_ = site_lists;
    if (!site_lists_changed_callback_.is_null())
      site_lists_changed_callback_.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  std::unique_ptr<MockSupervisedUserWhitelistInstaller> installer_;
  std::unique_ptr<SupervisedUserWhitelistService> service_;

  std::vector<scoped_refptr<SupervisedUserSiteList>> site_lists_;
  base::Closure site_lists_changed_callback_;
};

TEST_F(SupervisedUserWhitelistServiceTest, MergeEmpty) {
  service_->Init();

  syncer::SyncMergeResult result = service_->MergeDataAndStartSyncing(
      syncer::SUPERVISED_USER_WHITELISTS, syncer::SyncDataList(),
      std::unique_ptr<syncer::SyncChangeProcessor>(),
      std::unique_ptr<syncer::SyncErrorFactory>());
  EXPECT_FALSE(result.error().IsSet());
  EXPECT_EQ(0, result.num_items_added());
  EXPECT_EQ(0, result.num_items_modified());
  EXPECT_EQ(0, result.num_items_deleted());
  EXPECT_EQ(0, result.num_items_before_association());
  EXPECT_EQ(0, result.num_items_after_association());

  EXPECT_EQ(0u, installer_->registered_whitelists().size());
}

TEST_F(SupervisedUserWhitelistServiceTest, MergeExisting) {
  PrepareInitialStateAndPreferences();

  // Initialize the service. The whitelists should not be ready yet.
  service_->Init();
  EXPECT_EQ(0u, site_lists_.size());

  // Notify that whitelist A is ready.
  base::RunLoop run_loop;
  site_lists_changed_callback_ = run_loop.QuitClosure();
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath whitelist_path =
      test_data_dir.AppendASCII("whitelists/content_pack/site_list.json");
  installer_->NotifyWhitelistReady("aaaa", base::ASCIIToUTF16("Title"),
                                   base::FilePath(), whitelist_path);
  run_loop.Run();

  ASSERT_EQ(1u, site_lists_.size());
  EXPECT_EQ(base::ASCIIToUTF16("Title"), site_lists_[0]->title());
  EXPECT_EQ(4u, site_lists_[0]->patterns().size());

  // Do the initial merge. One item should be added (whitelist C), one should be
  // modified (whitelist B), and one item should be removed (whitelist A).
  syncer::SyncDataList initial_data;
  initial_data.push_back(
      SupervisedUserWhitelistService::CreateWhitelistSyncData(
          "bbbb", "Whitelist B New"));
  initial_data.push_back(
      SupervisedUserWhitelistService::CreateWhitelistSyncData(
          "cccc", "Whitelist C"));
  syncer::SyncMergeResult result = service_->MergeDataAndStartSyncing(
      syncer::SUPERVISED_USER_WHITELISTS, initial_data,
      std::unique_ptr<syncer::SyncChangeProcessor>(),
      std::unique_ptr<syncer::SyncErrorFactory>());
  EXPECT_FALSE(result.error().IsSet());
  EXPECT_EQ(1, result.num_items_added());
  EXPECT_EQ(1, result.num_items_modified());
  EXPECT_EQ(1, result.num_items_deleted());
  EXPECT_EQ(2, result.num_items_before_association());
  EXPECT_EQ(2, result.num_items_after_association());

  // Whitelist A (which was previously ready) should be removed now, and
  // whitelist B was never ready.
  EXPECT_EQ(0u, site_lists_.size());

  CheckFinalStateAndPreferences();
}

TEST_F(SupervisedUserWhitelistServiceTest, ApplyChanges) {
  PrepareInitialStateAndPreferences();

  service_->Init();

  // Process some changes.
  syncer::SyncChangeList changes;
  changes.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD,
      SupervisedUserWhitelistService::CreateWhitelistSyncData(
          "cccc", "Whitelist C")));
  changes.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      SupervisedUserWhitelistService::CreateWhitelistSyncData(
          "bbbb", "Whitelist B New")));
  changes.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_DELETE,
      SupervisedUserWhitelistService::CreateWhitelistSyncData(
          "aaaa", "Ignored")));
  syncer::SyncError error = service_->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.IsSet());

  EXPECT_EQ(0u, site_lists_.size());

  // If whitelist A now becomes ready, it should be ignored.
  installer_->NotifyWhitelistReady(
      "aaaa", base::ASCIIToUTF16("Title"), base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("/path/to/aaaa")));
  EXPECT_EQ(0u, site_lists_.size());

  CheckFinalStateAndPreferences();
}

TEST_F(SupervisedUserWhitelistServiceTest, GetAllSyncData) {
  PrepareInitialStateAndPreferences();

  syncer::SyncDataList sync_data =
      service_->GetAllSyncData(syncer::SUPERVISED_USER_WHITELISTS);
  ASSERT_EQ(2u, sync_data.size());
  const sync_pb::ManagedUserWhitelistSpecifics* whitelist =
      FindWhitelist(sync_data, "aaaa");
  ASSERT_TRUE(whitelist);
  EXPECT_EQ("Whitelist A", whitelist->name());
  whitelist = FindWhitelist(sync_data, "bbbb");
  ASSERT_TRUE(whitelist);
  EXPECT_EQ("Whitelist B", whitelist->name());
}
