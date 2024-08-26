// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"

namespace {

// Data providers with no local data will be filtered out.
std::vector<raw_ptr<const BatchUploadDataProvider>>
GetOrderedListOfDataProvider(
    const base::flat_map<BatchUploadDataType,
                         std::unique_ptr<BatchUploadDataProvider>>&
        data_providers_map) {
  // TODO(b/361340640): make the data type entry point the first one.
  std::vector<raw_ptr<const BatchUploadDataProvider>> providers_list;
  for (const auto& it : data_providers_map) {
    if (it.second->HasLocalData()) {
      providers_list.push_back(it.second.get());
    }
  }
  return providers_list;
}

}  // namespace

BatchUploadController::BatchUploadController(
    base::flat_map<BatchUploadDataType,
                   std::unique_ptr<BatchUploadDataProvider>> data_providers)
    : data_providers_(std::move(data_providers)) {
  for (const auto& it : data_providers_) {
    CHECK_EQ(it.first, it.second->GetDataType())
        << "Data providers data type and the keyed mapping value should always "
           "match.";
  }
}

BatchUploadController::~BatchUploadController() = default;

bool BatchUploadController::ShowDialog(
    BatchUploadDelegate& delegate,
    Browser* browser,
    base::OnceCallback<void(bool)> done_callback) {
  CHECK(done_callback);

  if (!HasLocalDataToShow()) {
    std::move(done_callback).Run(false);
    return false;
  }

  done_callback_ = std::move(done_callback);

  delegate.ShowBatchUploadDialog(
      browser, GetOrderedListOfDataProvider(data_providers_),
      /*complete_callback=*/
      base::BindOnce(&BatchUploadController::MoveItemsToAccountStorage,
                     base::Unretained(this)));
  return true;
}

void BatchUploadController::MoveItemsToAccountStorage(
    const base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::Id>>&
        items_to_move) {
  CHECK(done_callback_);

  // Delegate all the move actions to each data type provider.
  for (const auto& [type, items] : items_to_move) {
    auto it = data_providers_.find(type);
    CHECK(it != data_providers_.end());
    it->second->MoveToAccountStorage(items);
  }

  std::move(done_callback_).Run(/*move_requested=*/!items_to_move.empty());
}

bool BatchUploadController::HasLocalDataToShow() const {
  // As long as a data type has at least a single item to show, the dialog can
  // be shown.
  return std::ranges::any_of(data_providers_, [](const auto& entry) {
    return entry.second->HasLocalData();
  });
}
