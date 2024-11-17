// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

syncer::LocalDataItemModel MakeDummyLocalDataModel(size_t id) {
  syncer::LocalDataItemModel model;
  std::string id_string = base::ToString(id);
  model.id = id_string;
  model.title = "title_" + id_string;
  model.subtitle = "subtitle" + id_string;
  return model;
}

class BatchUploadDelegateMock : public BatchUploadDelegate {
 public:
  MOCK_METHOD(
      void,
      ShowBatchUploadDialog,
      (Browser * browser,
       std::vector<syncer::LocalDataDescription> local_data_description_list,
       BatchUploadService::EntryPoint entry_point,
       BatchUploadSelectedDataTypeItemsCallback complete_callback),
      (override));
};

}  // namespace

class BatchUploadServiceTest : public testing::Test {
 public:
  BatchUploadService& CreateService() {
    CHECK(!batch_upload_service_);
    // The real call is asynchronous, the mock calls the callback right away
    // which is simpler for testing.
    ON_CALL(mock_sync_service_, GetLocalDataDescriptions)
        .WillByDefault(
            [this](
                syncer::DataTypeSet types,
                base::OnceCallback<void(
                    std::map<syncer::DataType, syncer::LocalDataDescription>)>
                    callback) {
              std::move(callback).Run(returned_descriptions_);
            });

    std::unique_ptr<BatchUploadDelegateMock> delegate =
        std::make_unique<BatchUploadDelegateMock>();
    delegate_mock_ = delegate.get();
    batch_upload_service_ = std::make_unique<BatchUploadService>(
        identity_test_environment_.identity_manager(), &mock_sync_service_,
        std::move(delegate));

    return *batch_upload_service_;
  }

  signin::IdentityManager& identity_manager() {
    return *identity_test_environment_.identity_manager();
  }
  syncer::MockSyncService& sync_service_mock() { return mock_sync_service_; }
  BatchUploadDelegateMock& delegate_mock() {
    CHECK(delegate_mock_);
    return *delegate_mock_;
  }

  // Overrides `SyncService::GetLocalDataDescriptions()` by returned type in the
  // map by constructing dummy models.
  // Returns the expected returned description.
  const syncer::LocalDataDescription& SetReturnDescriptions(
      syncer::DataType type,
      size_t item_count) {
    syncer::LocalDataDescription& description = returned_descriptions_[type];
    description.type = type;
    description.local_data_models.clear();
    for (size_t i = 0; i < item_count; ++i) {
      description.local_data_models.push_back(MakeDummyLocalDataModel(i));
    }
    return description;
  }

  void SigninWithFullInfo(
      signin::ConsentLevel consent_level = signin::ConsentLevel::kSignin) {
    signin::IdentityManager* identity_manager =
        identity_test_environment_.identity_manager();
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@gmail.com", consent_level);
    ASSERT_FALSE(account_info.IsEmpty());

    account_info.full_name = "Joe Testing";
    account_info.given_name = "Joe";
    account_info.picture_url = "SOME_FAKE_URL";
    account_info.hosted_domain = kNoHostedDomainFound;
    account_info.locale = "en";
    ASSERT_TRUE(account_info.IsValid());
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::MockSyncService mock_sync_service_;

  std::unique_ptr<BatchUploadService> batch_upload_service_;
  raw_ptr<BatchUploadDelegateMock> delegate_mock_;

  std::map<syncer::DataType, syncer::LocalDataDescription>
      returned_descriptions_;
};

TEST_F(BatchUploadServiceTest, SignedOut) {
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;

  ASSERT_FALSE(
      identity_manager().HasPrimaryAccount(signin::ConsentLevel::kSignin));

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(0);
  EXPECT_CALL(delegate_mock(), ShowBatchUploadDialog(_, _, _, _)).Times(0);
  EXPECT_CALL(opened_callback, Run(false)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_FALSE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest, SignedPending) {
  SigninWithFullInfo();
  signin::SetInvalidRefreshTokenForPrimaryAccount(&identity_manager());
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(0);
  EXPECT_CALL(delegate_mock(), ShowBatchUploadDialog(_, _, _, _)).Times(0);
  EXPECT_CALL(opened_callback, Run(false)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_FALSE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest, Syncing) {
  SigninWithFullInfo();
  signin::SetPrimaryAccount(&identity_manager(), "email",
                            signin::ConsentLevel::kSync);
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(0);
  EXPECT_CALL(delegate_mock(), ShowBatchUploadDialog(_, _, _, _)).Times(0);
  EXPECT_CALL(opened_callback, Run(false)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_FALSE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest, NoLocalDataReturned) {
  SigninWithFullInfo();
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;

  // Lists the requested types.
  EXPECT_CALL(sync_service_mock(),
              GetLocalDataDescriptions(
                  syncer::DataTypeSet{syncer::DataType::PASSWORDS,
                                      syncer::DataType::CONTACT_INFO},
                  _))
      .Times(1);
  EXPECT_CALL(delegate_mock(), ShowBatchUploadDialog(_, _, _, _)).Times(0);
  EXPECT_CALL(opened_callback, Run(false)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_FALSE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest, EmptyLocalDataReturned) {
  SigninWithFullInfo();
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;
  SetReturnDescriptions(syncer::PASSWORDS, 0);

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(1);
  EXPECT_CALL(delegate_mock(), ShowBatchUploadDialog(_, _, _, _)).Times(0);
  EXPECT_CALL(opened_callback, Run(false)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_FALSE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest,
       LocalDataReturnedShowsDialogWithNonEmptyLocalData) {
  SigninWithFullInfo();
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;
  SetReturnDescriptions(syncer::PASSWORDS, 0);
  const syncer::LocalDataDescription& contact_info =
      SetReturnDescriptions(syncer::CONTACT_INFO, 2);

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(1);
  // Only expect `CONTACT_INFO` since return `PASSWORDS` are empty.
  EXPECT_CALL(
      delegate_mock(),
      ShowBatchUploadDialog(
          _, std::vector<syncer::LocalDataDescription>{contact_info}, _, _));
  EXPECT_CALL(opened_callback, Run(true)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_TRUE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest,
       MultipleLocalDataReturnedShowsDialogWithNonEmptyLocalData) {
  SigninWithFullInfo();
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;
  const syncer::LocalDataDescription& contact_info =
      SetReturnDescriptions(syncer::CONTACT_INFO, 2);
  const syncer::LocalDataDescription& passwords =
      SetReturnDescriptions(syncer::PASSWORDS, 3);

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(1);
  EXPECT_CALL(
      delegate_mock(),
      ShowBatchUploadDialog(
          _, std::vector<syncer::LocalDataDescription>{passwords, contact_info},
          _, _));
  EXPECT_CALL(opened_callback, Run(true)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_TRUE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest, LocalDataReturnedShowsDialogAndReturnIdToMove) {
  SigninWithFullInfo();
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;
  const syncer::LocalDataDescription& contact_infos =
      SetReturnDescriptions(syncer::CONTACT_INFO, 2);
  const syncer::LocalDataDescription& passwords =
      SetReturnDescriptions(syncer::PASSWORDS, 3);

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(1);
  std::vector<syncer::LocalDataDescription> expected_descriptions{
      passwords, contact_infos};
  BatchUploadSelectedDataTypeItemsCallback returned_complete_callback;
  EXPECT_CALL(delegate_mock(),
              ShowBatchUploadDialog(_, expected_descriptions, _, _))
      .WillOnce(
          [&](Browser* browser,
              const std::vector<syncer::LocalDataDescription>&
                  local_data_description_list,
              BatchUploadService::EntryPoint entry_point,
              BatchUploadSelectedDataTypeItemsCallback complete_callback) {
            returned_complete_callback = std::move(complete_callback);
          });
  EXPECT_CALL(opened_callback, Run(true)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_TRUE(service.IsDialogOpened());

  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      result{{syncer::PASSWORDS, {passwords.local_data_models[0].id}}};
  EXPECT_CALL(sync_service_mock(), TriggerLocalDataMigration(result)).Times(1);
  std::move(returned_complete_callback).Run(result);
  EXPECT_FALSE(service.IsDialogOpened());
}

TEST_F(BatchUploadServiceTest,
       LocalDataReturnedShowsDialogAndReturnNoIdToMove) {
  SigninWithFullInfo();
  BatchUploadService& service = CreateService();
  base::MockCallback<base::OnceCallback<void(bool)>> opened_callback;
  const syncer::LocalDataDescription& contact_infos =
      SetReturnDescriptions(syncer::CONTACT_INFO, 2);
  const syncer::LocalDataDescription& passwords =
      SetReturnDescriptions(syncer::PASSWORDS, 3);

  EXPECT_CALL(sync_service_mock(), GetLocalDataDescriptions(_, _)).Times(1);
  std::vector<syncer::LocalDataDescription> expected_descriptions{
      passwords, contact_infos};
  BatchUploadSelectedDataTypeItemsCallback returned_complete_callback;
  EXPECT_CALL(delegate_mock(),
              ShowBatchUploadDialog(_, expected_descriptions, _, _))
      .WillOnce(
          [&](Browser* browser,
              const std::vector<syncer::LocalDataDescription>&
                  local_data_description_list,
              BatchUploadService::EntryPoint entry_point,
              BatchUploadSelectedDataTypeItemsCallback complete_callback) {
            returned_complete_callback = std::move(complete_callback);
          });
  EXPECT_CALL(opened_callback, Run(true)).Times(1);
  service.OpenBatchUpload(
      nullptr, BatchUploadService::EntryPoint::kPasswordManagerSettings,
      opened_callback.Get());
  EXPECT_TRUE(service.IsDialogOpened());

  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      result;
  EXPECT_CALL(sync_service_mock(), TriggerLocalDataMigration(result)).Times(0);
  std::move(returned_complete_callback).Run(result);
  EXPECT_FALSE(service.IsDialogOpened());
}
