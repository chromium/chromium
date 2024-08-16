// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"

#include <memory>

#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Testing implementation of `BatchUploadDataProvider`.
class BatchUploadDataProviderFake : public BatchUploadDataProvider {
 public:
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

}  // namespace

TEST(BatchUploadControllerTest, EmptyController) {
  BatchUploadController controller({});
  // No providers means no local data; we do not show the dialog.
  EXPECT_FALSE(controller.ShowDialog());
}

TEST(BatchUploadControllerTest, ProviderWithLocalData) {
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>();
  provider->SetHasLocalData(true);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider));

  BatchUploadController controller(std::move(providers));
  // Having local data should show the dialog.
  EXPECT_TRUE(controller.ShowDialog());
}

TEST(BatchUploadControllerTest, ProvideWithoutLocalData) {
  std::unique_ptr<BatchUploadDataProviderFake> provider =
      std::make_unique<BatchUploadDataProviderFake>();
  provider->SetHasLocalData(false);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider));

  BatchUploadController controller(std::move(providers));
  // Even if the provider exists, having no data should not show the dialog.
  EXPECT_FALSE(controller.ShowDialog());
}

TEST(BatchUploadControllerTest, MultipleProvidersWithAndWithoutLocalData) {
  // Provider without data.
  std::unique_ptr<BatchUploadDataProviderFake> provider1 =
      std::make_unique<BatchUploadDataProviderFake>();
  provider1->SetHasLocalData(false);

  // Provider with data.
  std::unique_ptr<BatchUploadDataProviderFake> provider2 =
      std::make_unique<BatchUploadDataProviderFake>();
  provider2->SetHasLocalData(true);

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      providers;
  providers.insert_or_assign(BatchUploadDataType::kPasswords,
                             std::move(provider1));
  providers.insert_or_assign(BatchUploadDataType::kAddresses,
                             std::move(provider2));

  BatchUploadController controller(std::move(providers));
  // One provider with data is enough to allow showing the dialog.
  EXPECT_TRUE(controller.ShowDialog());
}
