// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Testing implementation of `BatchUploadDataProvider`.
class BatchUploadDataProviderFake : public BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProviderFake(BatchUploadDataType type)
      : BatchUploadDataProvider(type) {}

  void SetHasLocalData(bool has_local_data) {
    has_local_data_ = has_local_data;
  }

  bool HasLocalData() const override { return has_local_data_; }

  BatchUploadDataContainer GetLocalData() const override {
    BatchUploadDataContainer container(/*section_name_id=*/123,
                                       /*dialog_subtitle_id=*/456);
    if (has_local_data_) {
      // Add an arbitrary item.
      container.items.emplace_back();
    }
    return container;
  }

  bool MoveToAccountStorage(const std::vector<BatchUploadDataItemModel::Id>&
                                item_ids_to_move) override {
    return true;
  }

 private:
  bool has_local_data_ = false;
};

class BatchUploadDelegateMock : public BatchUploadDelegate {
 public:
  MOCK_METHOD(void,
              ShowBatchUploadDialog,
              (Browser * browser,
               const std::vector<raw_ptr<const BatchUploadDataProvider>>&
                   data_providers_list,
               SelectedDataTypeItemsCallback complete_callback),
              (override));
};

}  // namespace

TEST(BatchUploadControllerTest, EmptyController) {
  BatchUploadController controller({});
  BatchUploadDelegateMock mock;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // No providers means no local data; we do not show the dialog.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_callback, Run(false)).Times(1);
  // Not showing the bubble should still call the done_callback with no move
  // request.
  EXPECT_FALSE(controller.ShowDialog(mock, nullptr, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProviderWithLocalData) {
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kPasswords);
  BatchUploadDataProvider* provider_ptr = provider.get();
  provider->SetHasLocalData(true);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider));

  BatchUploadController controller(std::move(providers));
  BatchUploadDelegateMock mock;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // Having local data should show the dialog.
  std::vector<raw_ptr<const BatchUploadDataProvider>> expected_providers_list;
  // Provider has data and should be part of the input.
  expected_providers_list.emplace_back(provider_ptr);
  EXPECT_CALL(
      mock, ShowBatchUploadDialog(nullptr, expected_providers_list, testing::_))
      .Times(1);
  // The dialog was not closed yet, the `done_callback` should not be called.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProvideWithoutLocalData) {
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kPasswords);
  provider->SetHasLocalData(false);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider));

  BatchUploadController controller(std::move(providers));
  BatchUploadDelegateMock mock;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // Even if the provider exists, having no data should not show the dialog.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .Times(0);
  // Not showing the bubble should still call the done_callback with no move
  // request.
  EXPECT_CALL(mock_callback, Run(false)).Times(1);
  EXPECT_FALSE(controller.ShowDialog(mock, nullptr, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, MultipleProvidersWithAndWithoutLocalData) {
  // Provider without data.
  std::unique_ptr<BatchUploadDataProviderFake> provider1 =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kPasswords);
  provider1->SetHasLocalData(false);

  // Provider with data.
  std::unique_ptr<BatchUploadDataProviderFake> provider2 =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kAddresses);
  BatchUploadDataProviderFake* provider2_ptr = provider2.get();
  provider2->SetHasLocalData(true);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider1));
  providers.insert_or_assign(BatchUploadDataType::kAddresses,
                             std::move(provider2));

  BatchUploadController controller(std::move(providers));
  BatchUploadDelegateMock mock;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // One provider with data is enough to allow showing the dialog.
  std::vector<raw_ptr<const BatchUploadDataProvider>> expected_providers_list;
  // Only provider2 has data and should be part of the input.
  expected_providers_list.push_back(provider2_ptr);
  EXPECT_CALL(
      mock, ShowBatchUploadDialog(nullptr, expected_providers_list, testing::_))
      .Times(1);
  // The dialog was not closed yet, the `done_callback` should not be called.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, MultipleProvidersAllWithLocalData) {
  // Provider without data.
  std::unique_ptr<BatchUploadDataProviderFake> provider1 =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kPasswords);
  BatchUploadDataProviderFake* provider1_ptr = provider1.get();
  provider1->SetHasLocalData(true);

  // Provider with data.
  std::unique_ptr<BatchUploadDataProviderFake> provider2 =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kAddresses);
  BatchUploadDataProviderFake* provider2_ptr = provider2.get();
  provider2->SetHasLocalData(true);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider1));
  providers.insert_or_assign(BatchUploadDataType::kAddresses,
                             std::move(provider2));

  BatchUploadController controller(std::move(providers));
  BatchUploadDelegateMock mock;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // One provider with data is enough to allow showing the dialog.
  std::vector<raw_ptr<const BatchUploadDataProvider>> expected_providers_list;
  // Both providers have data and should be part of the input.
  // Provider1 has a higher priority than provider2, so it should be fist.
  EXPECT_LT(provider1_ptr->GetDataType(), provider2_ptr->GetDataType());
  expected_providers_list.push_back(provider1_ptr);
  expected_providers_list.push_back(provider2_ptr);
  EXPECT_CALL(
      mock, ShowBatchUploadDialog(nullptr, expected_providers_list, testing::_))
      .Times(1);
  // The dialog was not closed yet, the `done_callback` should not be called.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProviderWithItemsToMoveDoneCallback) {
  // Provider without data.
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kPasswords);
  BatchUploadDataProvider* provider_ptr = provider.get();
  provider->SetHasLocalData(true);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider));

  BatchUploadController controller(std::move(providers));
  BatchUploadDelegateMock mock;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // Close the dialog directly when shown, with returned items to move.
  std::vector<raw_ptr<const BatchUploadDataProvider>> expected_providers_list;
  // Provider has data and should be part of the input.
  expected_providers_list.push_back(provider_ptr);
  EXPECT_CALL(
      mock, ShowBatchUploadDialog(nullptr, expected_providers_list, testing::_))
      .WillOnce([](Browser* browser,
                   const std::vector<raw_ptr<const BatchUploadDataProvider>>&
                       data_providers_list,
                   SelectedDataTypeItemsCallback complete_callback) {
        ASSERT_EQ(data_providers_list.size(), 1u);
        EXPECT_TRUE(data_providers_list[0]->HasLocalData());

        base::flat_map<BatchUploadDataType,
                       std::vector<BatchUploadDataItemModel::Id>>
            selected_items;
        // Insert the first item of the first available provider.
        std::vector<BatchUploadDataItemModel::Id> item_ids;
        item_ids.emplace_back(
            data_providers_list[0]->GetLocalData().items[0].id);
        selected_items.insert_or_assign(data_providers_list[0]->GetDataType(),
                                        item_ids);
        std::move(complete_callback).Run(selected_items);
      });

  // Data was requested to be moved.
  EXPECT_CALL(mock_callback, Run(true)).Times(1);

  // One provider with data is enough to allow showing the dialog.
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, mock_callback.Get()));
}

TEST(BatchUploadControllerTest, ProviderWithoutItemsToMoveDoneCallback) {
  // Provider without data.
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>(
          BatchUploadDataType::kPasswords);
  provider->SetHasLocalData(true);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider));

  BatchUploadController controller(std::move(providers));
  BatchUploadDelegateMock mock;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // Close the dialog directly when shown, without returned items to move.
  EXPECT_CALL(mock, ShowBatchUploadDialog(nullptr, testing::_, testing::_))
      .WillOnce([](Browser* browser,
                   const std::vector<raw_ptr<const BatchUploadDataProvider>>&
                       data_providers_list,
                   SelectedDataTypeItemsCallback complete_callback) {
        // Empty items to move.
        std::move(complete_callback).Run({});
      });

  // No move request.
  EXPECT_CALL(mock_callback, Run(false)).Times(1);
  // One provider with data is enough to allow showing the dialog.
  EXPECT_TRUE(controller.ShowDialog(mock, nullptr, mock_callback.Get()));
}
