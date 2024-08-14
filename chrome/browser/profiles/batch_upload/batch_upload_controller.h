// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_CONTROLLER_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_CONTROLLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"

// Data types that integrates with the Batch Upload and can be displayed in the
// dialog.
// Ordered by priority as the enum will be used in a map. The priority order
// controls the order in which the data type section is displayed in the dialog.
enum class BatchUploadDataType {
  kPasswords,
  kAddresses,
};

// Controller that manages the information displayed in the Batch Upload dialog.
// Contains the interfaces that communicates with the different data types,
// getting the exact information to display (retrieving the local data per
// type), and process the user input from the dialog, to be redirected to the
// right data type to move the items to the account storage.
class BatchUploadController {
 public:
  explicit BatchUploadController(
      base::flat_map<BatchUploadDataType,
                     std::unique_ptr<BatchUploadDataProvider>> data_providers);

  // Attempts to show the Batch Upload dialog based on the data it currently
  // has.
  bool ShowDialog();

  ~BatchUploadController();

 private:
  // Success callback of the dialog view, allows proceeding with the move of the
  // selected data items per data type to the account storages.
  void MoveItemsToAccountStorage(
      const base::flat_map<BatchUploadDataType,
                           std::vector<BatchUploadDataItemModel::Id>>&
          items_to_move);

  // Whether there exist a current local data item of any type.
  bool HasLocalDataToShow() const;

  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      data_providers_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_CONTROLLER_H_
