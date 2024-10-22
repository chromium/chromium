// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::map<syncer::DataType,
               std::vector<syncer::LocalDataItemModel::DataId>>
    kEmptySelectedMap;

// Helper alias to a mock callback of the result of the Batch Upload Dialog.
using MockBatchUploadDialogResultCallback =
    base::MockCallback<BatchUploadSelectedDataTypeItemsCallback>;

// Testing implementation of `BatchUploadDataProvider`.
class BatchUploadDataProviderFake : public BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProviderFake(syncer::DataType type)
      : BatchUploadDataProvider(type) {}

  void SetHasLocalData(bool has_local_data) {
    has_local_data_ = has_local_data;
  }

  bool HasLocalData() const override { return has_local_data_; }

  syncer::LocalDataDescription GetLocalData() const override {
    syncer::LocalDataDescription local_data_description;
    local_data_description.type = GetDataType();
    if (has_local_data_) {
      // Add an arbitrary item.
      local_data_description.local_data_models.emplace_back();
    }
    return local_data_description;
  }

  bool MoveToAccountStorage(
      const std::vector<syncer::LocalDataItemModel::DataId>& item_ids_to_move)
      override {
    return true;
  }

 private:
  bool has_local_data_ = false;
};

class BatchUploadDelegateMock : public BatchUploadDelegate {
 public:
  MOCK_METHOD(
      void,
      ShowBatchUploadDialog,
      (Browser * browser,
       std::vector<syncer::LocalDataDescription> local_data_description_list,
       BatchUploadSelectedDataTypeItemsCallback complete_callback),
      (override));
};

}  // namespace

TEST(BatchUploadControllerTest, EmptyController) {
  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // No local_descriptions means no local data; we do not show the dialog.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  // Not showing the bubble should still call the done_callback with no move
  // request.
  EXPECT_FALSE(controller.ShowDialog(mock, nullptr, {}, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProviderWithLocalData) {
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::PASSWORDS);
  provider->SetHasLocalData(true);

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // Having local data should show the dialog.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;
  // local_description has data and should be part of the input.
  expected_descriptions_list.push_back(provider->GetLocalData());
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .WillOnce(
          [&expected_descriptions_list](
              Browser* browser,
              std::vector<syncer::LocalDataDescription>
                  local_data_description_list,
              BatchUploadSelectedDataTypeItemsCallback complete_callback) {
            EXPECT_EQ(expected_descriptions_list, local_data_description_list);
          });

  // The dialog was not closed yet, the `done_callback` should not be called.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(provider->GetDataType(), provider->GetLocalData());
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProvideWithoutLocalData) {
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::PASSWORDS);
  provider->SetHasLocalData(false);

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // Even if the provider exists, having no data should not show the dialog.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .Times(0);
  // Not showing the bubble should still call the done_callback with an empty
  // map..
  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(provider->GetDataType(), provider->GetLocalData());
  EXPECT_FALSE(controller.ShowDialog(mock, nullptr, std::move(input),
                                     mock_callback.Get()));
}

TEST(BatchUploadControllerTest, MultipleProvidersWithAndWithoutLocalData) {
  // Provider without data.
  std::unique_ptr<BatchUploadDataProviderFake> provider1 =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::PASSWORDS);
  provider1->SetHasLocalData(false);

  // Provider with data.
  std::unique_ptr<BatchUploadDataProviderFake> provider2 =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::CONTACT_INFO);
  provider2->SetHasLocalData(true);

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // One local_description with data is enough to allow showing the dialog.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;
  // Only local_description from provider2 has data and should be part of the
  // input.
  expected_descriptions_list.push_back(provider2->GetLocalData());
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .WillOnce(
          [&expected_descriptions_list](
              Browser* browser,
              std::vector<syncer::LocalDataDescription>
                  local_data_description_list,
              BatchUploadSelectedDataTypeItemsCallback complete_callback) {
            EXPECT_EQ(expected_descriptions_list, local_data_description_list);
          });

  // The dialog was not closed yet, the `done_callback` should not be called.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(provider1->GetDataType(), provider1->GetLocalData());
  input.insert_or_assign(provider2->GetDataType(), provider2->GetLocalData());
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, MultipleProvidersAllWithLocalData) {
  // Provider with data.
  std::unique_ptr<BatchUploadDataProviderFake> provider1 =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::PASSWORDS);
  provider1->SetHasLocalData(true);

  // Provider with data.
  std::unique_ptr<BatchUploadDataProviderFake> provider2 =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::CONTACT_INFO);
  provider2->SetHasLocalData(true);

  syncer::LocalDataDescription local_description1 = provider1->GetLocalData();
  syncer::LocalDataDescription local_description2 = provider2->GetLocalData();

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // One provider with data is enough to allow showing the dialog.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;

  // Both providers have data and should be part of the input.
  // Provider1 has a higher priority than provider2, so it should be fist.
  EXPECT_LT(provider1->GetDataType(), provider2->GetDataType());
  expected_descriptions_list.push_back(provider1->GetLocalData());
  expected_descriptions_list.push_back(provider2->GetLocalData());
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .WillOnce(
          [&expected_descriptions_list](
              Browser* browser,
              std::vector<syncer::LocalDataDescription>
                  local_data_description_list,
              BatchUploadSelectedDataTypeItemsCallback complete_callback) {
            EXPECT_EQ(expected_descriptions_list, local_data_description_list);
          });

  // The dialog was not closed yet, the `done_callback` should not be called.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(provider1->GetDataType(), provider1->GetLocalData());
  input.insert_or_assign(provider2->GetDataType(), provider2->GetLocalData());
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProviderWithItemsToMoveDoneCallback) {
  // Provider with data.
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::PASSWORDS);
  provider->SetHasLocalData(true);

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // Extract the first item id.
  syncer::LocalDataDescription local_description = provider->GetLocalData();
  syncer::LocalDataItemModel::DataId first_item_id =
      local_description.local_data_models[0].id;

  // Close the dialog directly when shown, with returned items to move.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;
  // Provider has data and should be part of the input.
  expected_descriptions_list.push_back(std::move(local_description));
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .WillOnce(
          [&first_item_id](
              Browser* browser,
              const std::vector<syncer::LocalDataDescription>&
                  local_data_description_list,
              BatchUploadSelectedDataTypeItemsCallback complete_callback) {
            ASSERT_EQ(local_data_description_list.size(), 1u);
            EXPECT_FALSE(
                local_data_description_list[0].local_data_models.empty());

            std::map<syncer::DataType,
                     std::vector<syncer::LocalDataItemModel::DataId>>
                selected_items;
            // Insert the first item of the first available local_description.
            std::vector<syncer::LocalDataItemModel::DataId> item_ids;
            item_ids.emplace_back(first_item_id);
            selected_items.insert_or_assign(local_data_description_list[0].type,
                                            item_ids);
            std::move(complete_callback).Run(selected_items);
          });

  // Data was requested to be moved. Map contains the first item id.
  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      expected_result_map{
          {provider->GetDataType(),
           std::vector<syncer::LocalDataItemModel::DataId>{first_item_id}}};
  EXPECT_CALL(mock_callback, Run(expected_result_map)).Times(1);

  // One provider with data is enough to allow showing the dialog.
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(provider->GetDataType(), provider->GetLocalData());
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProviderWithoutItemsToMoveDoneCallback) {
  // Provider without data.
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          syncer::DataType::PASSWORDS);
  provider->SetHasLocalData(true);

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // Close the dialog directly when shown, without returned items to move.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .WillOnce([](Browser* browser,
                   const std::vector<syncer::LocalDataDescription>&
                       local_data_description_list,
                   BatchUploadSelectedDataTypeItemsCallback complete_callback) {
        // Empty items to move.
        std::move(complete_callback).Run({});
      });

  // No move request.
  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  // One provider with data is enough to allow showing the dialog.
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(provider->GetDataType(), provider->GetLocalData());
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}
