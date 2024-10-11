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

// Data containers with no local data will be filtered out.
std::vector<BatchUploadDataContainer> GetOrderedListOfNonEmptyDataContainers(
    base::flat_map<BatchUploadDataType, BatchUploadDataContainer>
        data_containers_map) {
  // TODO(b/361340640): make the data type entry point the first one.
  std::vector<BatchUploadDataContainer> containers_list;
  for (auto& it : data_containers_map) {
    if (!it.second.items.empty()) {
      containers_list.push_back(std::move(it.second));
    }
  }
  return containers_list;
}

// Whether there exist a current local data item of any type.
bool HasLocalDataToShow(
    const base::flat_map<BatchUploadDataType, BatchUploadDataContainer>&
        data_containers) {
  // As long as a data type has at least a single item to show, the dialog can
  // be shown.
  return std::ranges::any_of(data_containers, [](const auto& data_container) {
    return !data_container.second.items.empty();
  });
}

}  // namespace

BatchUploadController::BatchUploadController() = default;

BatchUploadController::~BatchUploadController() = default;

bool BatchUploadController::ShowDialog(
    BatchUploadDelegate& delegate,
    Browser* browser,
    base::flat_map<BatchUploadDataType, BatchUploadDataContainer>
        data_containers,
    SelectedDataTypeItemsCallback selected_items_callback) {
  CHECK(selected_items_callback);

  for (const auto& it : data_containers) {
    CHECK_EQ(it.first, it.second.type) << "Data containers data type and the "
                                          "keyed mapping value should always "
                                          "match.";
  }

  if (!HasLocalDataToShow(data_containers)) {
    std::move(selected_items_callback).Run({});
    return false;
  }

  selected_items_callback_ = std::move(selected_items_callback);

  delegate.ShowBatchUploadDialog(
      browser,
      GetOrderedListOfNonEmptyDataContainers(std::move(data_containers)),
      /*complete_callback=*/
      base::BindOnce(&BatchUploadController::MoveItemsToAccountStorage,
                     base::Unretained(this)));
  return true;
}

void BatchUploadController::MoveItemsToAccountStorage(
    const base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::DataId>>&
        items_to_move) {
  CHECK(selected_items_callback_);
  std::move(selected_items_callback_).Run(items_to_move);
}
