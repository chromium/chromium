// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload.h"

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/ui/browser.h"

namespace {

// Temporary Dummy implementation. All IDs provided are arbitrary.
// TODO(b/359146556): remove when actual providers are implemented.
class DummyBatchUploadDataProvider : public BatchUploadDataProvider {
 public:
  bool HasLocalData() const override { return true; }

  BatchUploadDataContainer GetLocalData() const override {
    BatchUploadDataContainer container(/*section_name_id=*/123,
                                       /*dialog_subtitle_id=*/456);
    container.items.push_back(
        BatchUploadDataItemModel{.id = BatchUploadDataItemModel::Id(321),
                                 .title = "title",
                                 .subtitle = "subtitle"});
    return container;
  }

  bool MoveToAccountStorage(const std::vector<BatchUploadDataItemModel::Id>&
                                item_ids_to_move) override {
    return true;
  }
};

// Returns a dummy implementation.
// TODO(b/359146556): remove when actual providers are implemented.
std::unique_ptr<BatchUploadDataProvider> MakeDummyBatchUploadDataProvider() {
  return std::make_unique<DummyBatchUploadDataProvider>();
}

// Gets the `BatchUploadDataProvider` of a single data type. Can also be used in
// order to know if a specific data type entry point for the BatchUpload should
// be visible or not, without needing to create the whole BatchUpload logic.
// The returned `BatchUploadDataProvider` should not be null.
std::unique_ptr<BatchUploadDataProvider> GetBatchUploadDataProvider(
    Profile& profile,
    BatchUploadDataType type) {
  switch (type) {
    case BatchUploadDataType::kPasswords:
    case BatchUploadDataType::kAddresses:
      // TODO(b/359146556): real implementations to be added per data type.
      return MakeDummyBatchUploadDataProvider();
  }
}

// Helper function to get the map of all `BatchUploadDataProvider` of all data
// types that can have local data that can be displayed by the BatchUpload
// dialog.
base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
GetBatchUploadDataProviderMap(Profile& profile) {
  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      data_providers;

  data_providers[BatchUploadDataType::kPasswords] =
      GetBatchUploadDataProvider(profile, BatchUploadDataType::kPasswords);
  data_providers[BatchUploadDataType::kAddresses] =
      GetBatchUploadDataProvider(profile, BatchUploadDataType::kAddresses);

  return data_providers;
}

}  // namespace

bool OpenBatchUpload(Browser* browser) {
  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      data_providers = GetBatchUploadDataProviderMap(*browser->profile());
  // TODO(b/359146413): Tackle this task when implementing the main view.
  // Currently this does nothing as there `BatchUploadController::ShowDialog()`
  // only has a dummy implementation. `BatchUploadController` also needs to have
  // a concrete owner while the dialog is shown -- there are multiple options
  // for now:
  // - As a BrowserUserData
  // - As a keyed service
  // - As part of the dialog that will be shown that is itself owned by the
  // views framework
  BatchUploadController controller(std::move(data_providers));
  return controller.ShowDialog();
}

bool ShouldShowBatchUploadEntryPointForDataType(Profile& profile,
                                                BatchUploadDataType type) {
  std::unique_ptr<BatchUploadDataProvider> local_data_provider =
      GetBatchUploadDataProvider(profile, type);
  return local_data_provider->HasLocalData();
}
