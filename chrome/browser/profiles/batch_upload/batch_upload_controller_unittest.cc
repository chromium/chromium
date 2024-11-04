// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
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

syncer::LocalDataDescription GetLocalDataDescription(syncer::DataType type,
                                                     bool with_local_data) {
  syncer::LocalDataDescription local_data_description;
  local_data_description.type = type;
  if (with_local_data) {
    // Add an arbitrary item.
    local_data_description.local_data_models.emplace_back();
  }
  return local_data_description;
}

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

TEST(BatchUploadControllerTest, ControllerWithNoDataDescription) {
  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // No local_descriptions means no local data; we do not show the dialog.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .Times(0);
  // Callback is not called as the controller immedialy returns false.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  // Not showing the bubble should still call the done_callback with no move
  // request.
  EXPECT_FALSE(controller.ShowDialog(mock, nullptr, {}, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, DataDescriptionWithLocalData) {
  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;
  syncer::LocalDataDescription local_data = GetLocalDataDescription(
      syncer::DataType::PASSWORDS, /*with_local_data=*/true);

  // Having local data should show the dialog.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;
  // local_description has data and should be part of the input.
  expected_descriptions_list.push_back(local_data);
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
  input.insert_or_assign(local_data.type, local_data);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, DataDescriptionWithoutLocalData) {
  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  syncer::LocalDataDescription local_data = GetLocalDataDescription(
      syncer::DataType::PASSWORDS, /*with_local_data=*/false);

  // Even if the description exists, having no data should not show the dialog.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .Times(0);
  // Not showing the bubble should just return directly without invoking the
  // callback.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(local_data.type, local_data);
  EXPECT_FALSE(controller.ShowDialog(mock, nullptr, std::move(input),
                                     mock_callback.Get()));
}

TEST(BatchUploadControllerTest,
     MultipleDataDescriptionsWithAndWithoutLocalData) {
  // Description without data.
  syncer::LocalDataDescription local_data1 = GetLocalDataDescription(
      syncer::DataType::PASSWORDS, /*with_local_data=*/false);

  // Description with data.
  syncer::LocalDataDescription local_data2 = GetLocalDataDescription(
      syncer::DataType::CONTACT_INFO, /*with_local_data=*/true);

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // One description with data is enough to allow showing the dialog.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;
  // Only `local_data2` has data and should be part of the
  // input.
  expected_descriptions_list.push_back(local_data2);
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
  input.insert_or_assign(local_data1.type, local_data1);
  input.insert_or_assign(local_data2.type, local_data2);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, MultipleDataDescriptionsAllWithLocalData) {
  // Description with data.
  syncer::LocalDataDescription local_data1 = GetLocalDataDescription(
      syncer::DataType::CONTACT_INFO, /*with_local_data=*/true);

  // Description with data.
  syncer::LocalDataDescription local_data2 = GetLocalDataDescription(
      syncer::DataType::PASSWORDS, /*with_local_data=*/true);

  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  // One description with data is enough to allow showing the dialog.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;
  // Both descriptions have data and should be part of the input.
  // `local_data2`'s type has a higher priority than `local_data1`'s type, so it
  // should be first.
  EXPECT_LT(local_data2.type, local_data1.type);
  expected_descriptions_list.push_back(local_data2);
  expected_descriptions_list.push_back(local_data1);
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
  input.insert_or_assign(local_data1.type, local_data1);
  input.insert_or_assign(local_data2.type, local_data2);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, WithItemsToMoveDoneCallback) {
  BatchUploadController controller;
  BatchUploadDelegateMock mock;
  MockBatchUploadDialogResultCallback mock_callback;

  syncer::LocalDataDescription local_data = GetLocalDataDescription(
      syncer::DataType::PASSWORDS, /*with_local_data=*/true);
  // Extract the first item id.
  syncer::LocalDataItemModel::DataId first_item_id =
      local_data.local_data_models[0].id;

  // Close the dialog directly when shown, with returned items to move.
  std::vector<syncer::LocalDataDescription> expected_descriptions_list;
  // `local_data` has data and should be part of the input.
  expected_descriptions_list.push_back(local_data);
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
            // Insert the first item of the first available local_data.
            std::vector<syncer::LocalDataItemModel::DataId> item_ids;
            item_ids.emplace_back(first_item_id);
            selected_items.insert_or_assign(local_data_description_list[0].type,
                                            item_ids);
            std::move(complete_callback).Run(selected_items);
          });

  // Data was requested to be moved. Map contains the first item id.
  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      expected_result_map{
          {local_data.type,
           {syncer::LocalDataItemModel::DataId{first_item_id}}}};
  EXPECT_CALL(mock_callback, Run(expected_result_map)).Times(1);

  // One description with data is enough to allow showing the dialog.
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(local_data.type, local_data);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}

TEST(BatchUploadControllerTest, WithoutItemsToMoveDoneCallback) {
  // Description with data.
  syncer::LocalDataDescription local_data = GetLocalDataDescription(
      syncer::DataType::PASSWORDS, /*with_local_data=*/true);

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
  // One description with data is enough to allow showing the dialog.
  std::map<syncer::DataType, syncer::LocalDataDescription> input;
  input.insert_or_assign(local_data.type, local_data);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, std::move(input),
                                    mock_callback.Get()));
}
