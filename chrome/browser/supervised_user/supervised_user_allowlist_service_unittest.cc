// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_allowlist_service.h"

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

  const std::set<std::string>& registered_allowlists() {
    return registered_allowlists_;
  }

  void NotifyAllowlistReady(const std::string& crx_id,
                            const base::string16& title,
                            const base::FilePath& large_icon_path,
                            const base::FilePath& allowlist_path) {
    for (const auto& callback : ready_callbacks_)
      callback.Run(crx_id, title, large_icon_path, allowlist_path);
  }

  // SupervisedUserWhitelistInstaller implementation:
  void RegisterComponents() override {}

  void Subscribe(WhitelistReadyCallback callback) override {
    ready_callbacks_.push_back(callback);
  }

  void RegisterWhitelist(const std::string& client_id,
                         const std::string& crx_id,
                         const std::string& name) override {
    EXPECT_EQ(kClientId, client_id);
    EXPECT_FALSE(AllowlistIsRegistered(crx_id)) << crx_id;
    registered_allowlists_.insert(crx_id);
  }

  void UnregisterWhitelist(const std::string& client_id,
                           const std::string& crx_id) override {
    EXPECT_EQ(kClientId, client_id);
    EXPECT_TRUE(AllowlistIsRegistered(crx_id)) << crx_id;
    registered_allowlists_.erase(crx_id);
  }

 private:
  bool AllowlistIsRegistered(const std::string& crx_id) {
    return registered_allowlists_.count(crx_id) > 0;
  }

  std::set<std::string> registered_allowlists_;
  std::vector<WhitelistReadyCallback> ready_callbacks_;
};

}  // namespace

class SupervisedUserAllowlistServiceTest : public testing::Test {
 public:
  SupervisedUserAllowlistServiceTest()
      : installer_(new MockSupervisedUserWhitelistInstaller),
        service_(new SupervisedUserAllowlistService(profile_.GetPrefs(),
                                                    installer_.get(),
                                                    kClientId)) {
    service_->AddSiteListsChangedCallback(
        base::Bind(&SupervisedUserAllowlistServiceTest::OnSiteListsChanged,
                   base::Unretained(this)));
  }

 protected:
  void PrepareInitialStateAndPreferences() {
    // Create two allowlists.
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                prefs::kSupervisedUserAllowlists);
    base::DictionaryValue* dict = update.Get();

    std::unique_ptr<base::DictionaryValue> allowlist_dict(
        new base::DictionaryValue);
    allowlist_dict->SetString("name", "Allowlist A");
    dict->Set("aaaa", std::move(allowlist_dict));

    allowlist_dict.reset(new base::DictionaryValue);
    allowlist_dict->SetString("name", "Allowlist B");
    dict->Set("bbbb", std::move(allowlist_dict));

    installer_->RegisterWhitelist(kClientId, "aaaa", "Allowlist A");
    installer_->RegisterWhitelist(kClientId, "bbbb", "Allowlist B");
  }

  void CheckFinalStateAndPreferences() {
    EXPECT_EQ(2u, installer_->registered_allowlists().size());
    EXPECT_EQ(1u, installer_->registered_allowlists().count("bbbb"));
    EXPECT_EQ(1u, installer_->registered_allowlists().count("cccc"));

    const base::DictionaryValue* dict =
        profile_.GetPrefs()->GetDictionary(prefs::kSupervisedUserAllowlists);
    EXPECT_EQ(2u, dict->size());
    const base::DictionaryValue* allowlist_dict = nullptr;
    ASSERT_TRUE(dict->GetDictionary("bbbb", &allowlist_dict));
    std::string name;
    ASSERT_TRUE(allowlist_dict->GetString("name", &name));
    EXPECT_EQ("Allowlist B New", name);
    ASSERT_TRUE(dict->GetDictionary("cccc", &allowlist_dict));
    ASSERT_TRUE(allowlist_dict->GetString("name", &name));
    EXPECT_EQ("Allowlist C", name);
  }

  const sync_pb::ManagedUserWhitelistSpecifics* FindAllowlist(
      const syncer::SyncDataList& data_list,
      const std::string& id) {
    for (const syncer::SyncData& data : data_list) {
      const sync_pb::ManagedUserWhitelistSpecifics& allowlist =
          data.GetSpecifics().managed_user_whitelist();
      if (allowlist.id() == id)
        return &allowlist;
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
  std::unique_ptr<SupervisedUserAllowlistService> service_;

  std::vector<scoped_refptr<SupervisedUserSiteList>> site_lists_;
  base::Closure site_lists_changed_callback_;
};

TEST_F(SupervisedUserAllowlistServiceTest, MergeEmpty) {
  service_->Init();

  ASSERT_TRUE(service_
                  ->GetAllSyncDataForTesting(
                      syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS)
                  .empty());
  base::Optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS, syncer::SyncDataList(),
      std::unique_ptr<syncer::SyncChangeProcessor>(),
      std::unique_ptr<syncer::SyncErrorFactory>());
  EXPECT_TRUE(service_
                  ->GetAllSyncDataForTesting(
                      syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS)
                  .empty());
  EXPECT_FALSE(error.has_value());

  EXPECT_EQ(0u, installer_->registered_allowlists().size());
}

TEST_F(SupervisedUserAllowlistServiceTest, MergeExisting) {
  PrepareInitialStateAndPreferences();

  // Initialize the service. The allowlists should not be ready yet.
  service_->Init();
  EXPECT_EQ(0u, site_lists_.size());

  // Notify that allowlist A is ready.
  base::RunLoop run_loop;
  site_lists_changed_callback_ = run_loop.QuitClosure();
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath allowlist_path =
      test_data_dir.AppendASCII("allowlists/content_pack/site_list.json");
  installer_->NotifyAllowlistReady("aaaa", base::ASCIIToUTF16("Title"),
                                   base::FilePath(), allowlist_path);
  run_loop.Run();

  ASSERT_EQ(1u, site_lists_.size());
  EXPECT_EQ(base::ASCIIToUTF16("Title"), site_lists_[0]->title());
  EXPECT_EQ(4u, site_lists_[0]->patterns().size());

  // Do the initial merge. One item should be added (allowlist C), one should be
  // modified (allowlist B), and one item should be removed (allowlist A).
  syncer::SyncDataList initial_data;
  initial_data.push_back(
      SupervisedUserAllowlistService::CreateAllowlistSyncData(
          "bbbb", "Allowlist B New"));
  initial_data.push_back(
      SupervisedUserAllowlistService::CreateAllowlistSyncData("cccc",
                                                              "Allowlist C"));
  ASSERT_EQ(2u, service_
                    ->GetAllSyncDataForTesting(
                        syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS)
                    .size());
  base::Optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS, initial_data,
      std::unique_ptr<syncer::SyncChangeProcessor>(),
      std::unique_ptr<syncer::SyncErrorFactory>());
  EXPECT_EQ(2u, service_
                    ->GetAllSyncDataForTesting(
                        syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS)
                    .size());
  EXPECT_FALSE(error.has_value());

  // Allowlist A (which was previously ready) should be removed now, and
  // allowlist B was never ready.
  EXPECT_EQ(0u, site_lists_.size());

  CheckFinalStateAndPreferences();
}

TEST_F(SupervisedUserAllowlistServiceTest, ApplyChanges) {
  PrepareInitialStateAndPreferences();

  service_->Init();

  // Process some changes.
  syncer::SyncChangeList changes;
  changes.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD,
      SupervisedUserAllowlistService::CreateAllowlistSyncData("cccc",
                                                              "Allowlist C")));
  changes.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      SupervisedUserAllowlistService::CreateAllowlistSyncData(
          "bbbb", "Allowlist B New")));
  changes.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_DELETE,
      SupervisedUserAllowlistService::CreateAllowlistSyncData("aaaa",
                                                              "Ignored")));
  base::Optional<syncer::ModelError> error =
      service_->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.has_value());

  EXPECT_EQ(0u, site_lists_.size());

  // If allowlist A now becomes ready, it should be ignored.
  installer_->NotifyAllowlistReady(
      "aaaa", base::ASCIIToUTF16("Title"), base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("/path/to/aaaa")));
  EXPECT_EQ(0u, site_lists_.size());

  CheckFinalStateAndPreferences();
}
