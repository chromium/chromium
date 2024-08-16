// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"

#include <algorithm>

#include "base/check_op.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"

BatchUploadController::BatchUploadController(
    base::flat_map<BatchUploadDataType,
                   std::unique_ptr<BatchUploadDataProvider>> data_providers)
    : data_providers_(std::move(data_providers)) {}

BatchUploadController::~BatchUploadController() = default;

bool BatchUploadController::ShowDialog() {
  if (!HasLocalDataToShow()) {
    return false;
  }

  // TODO(b/359146413): create the main dialog view, feeds in the list of
  // sections and items that needs to be displayed. Also providing the success
  // callback that returns the selected items per section, using
  // `MoveItemsToAccountStorage()`.
  return true;
}

void BatchUploadController::MoveItemsToAccountStorage(
    const base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::Id>>&
        items_to_move) {
  // Delegate all the move actions to each data type provider.
  for (const auto& [type, items] : items_to_move) {
    auto it = data_providers_.find(type);
    CHECK(it != data_providers_.end());
    it->second->MoveToAccountStorage(items);
  }
}

bool BatchUploadController::HasLocalDataToShow() const {
  // As long as a data type has at least a single item to show, the dialog can
  // be shown.
  return std::ranges::any_of(data_providers_, [](const auto& entry) {
    return entry.second->HasLocalData();
  });
}
