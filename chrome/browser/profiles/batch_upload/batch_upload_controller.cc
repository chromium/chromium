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
#include "components/sync/service/local_data_description.h"

namespace {

// Data descriptions with no local data will be filtered out.
std::vector<syncer::LocalDataDescription>
GetOrderedListOfNonEmptyDataDescriptions(
    std::map<syncer::DataType, syncer::LocalDataDescription>
        local_data_descriptions_map) {
  // TODO(b/361340640): make the data type entry point the first one.
  // TODO(crbug.com/374133537): Use `kBatchUploadOrderedAvailableTypes` types
  // order to reorder the returned list for display order.
  std::vector<syncer::LocalDataDescription> local_data_description_list;
  for (auto& [type, local_data_description] : local_data_descriptions_map) {
    if (!local_data_description.local_data_models.empty()) {
      CHECK_EQ(type, local_data_description.type)
          << "Non empty data description's data type and the keyed mapping "
             "value should always match.";

      local_data_description_list.push_back(std::move(local_data_description));
    }
  }
  return local_data_description_list;
}

// Whether there exist a current local data item of any type.
bool HasLocalDataToShow(
    const std::map<syncer::DataType, syncer::LocalDataDescription>&
        local_data_descriptions) {
  // As long as a data type has at least a single item to show, the dialog can
  // be shown.
  return std::ranges::any_of(
      local_data_descriptions, [](const auto& local_data_description) {
        return !local_data_description.second.local_data_models.empty();
      });
}

}  // namespace

BatchUploadController::BatchUploadController() = default;

BatchUploadController::~BatchUploadController() = default;

bool BatchUploadController::ShowDialog(
    BatchUploadDelegate& delegate,
    Browser* browser,
    std::map<syncer::DataType, syncer::LocalDataDescription>
        local_data_descriptions,
    BatchUploadSelectedDataTypeItemsCallback selected_items_callback) {
  CHECK(selected_items_callback);

  if (!HasLocalDataToShow(local_data_descriptions)) {
    std::move(selected_items_callback).Run({});
    return false;
  }

  selected_items_callback_ = std::move(selected_items_callback);

  delegate.ShowBatchUploadDialog(
      browser,
      GetOrderedListOfNonEmptyDataDescriptions(
          std::move(local_data_descriptions)),
      /*complete_callback=*/
      base::BindOnce(&BatchUploadController::MoveItemsToAccountStorage,
                     base::Unretained(this)));
  return true;
}

void BatchUploadController::MoveItemsToAccountStorage(
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>&
        items_to_move) {
  CHECK(selected_items_callback_);
  std::move(selected_items_callback_).Run(items_to_move);
}
