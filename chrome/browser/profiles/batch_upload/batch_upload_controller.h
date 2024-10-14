// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_CONTROLLER_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_CONTROLLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"

class Browser;

// Data types that integrates with the Batch Upload and can be displayed in the
// dialog.
// Ordered by priority as the enum will be used in a map. The priority order
// controls the order in which the data type section is displayed in the dialog.
// This enum will be iterated over to get the `BatchUploadDataContainers`.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BatchUploadDataType {
  kPasswords = 0,
  kAddresses = 1,

  kMaxValue = kAddresses,
};

// Controller that manages the information displayed in the Batch Upload dialog.
// Receoves the different data types models to display, and triggers the Ui
// creation. Then redirects the user input from the dialog, to the service for
// processing.
//
// TODO(crbug.com/372827366): Consider removing the controller as it does not
// provide much anymore. It only triggers the Ui creation and by getting
// information from the service (and performing minor checks that can be moved
// to the service) and redirects results to it. It is still helpful for unit
// testing.
class BatchUploadController {
 public:
  BatchUploadController();
  ~BatchUploadController();

  // Attempts to show the Batch Upload dialog based on the data it currently
  // has. `selected_items_callback` is called whenever the dialog is closed. The
  // resulting map of the callback indicates which data were requested to move
  // to the account storage. `browser` must not be null, but may be null in some
  // tests.
  bool ShowDialog(BatchUploadDelegate& delegate,
                  Browser* browser,
                  base::flat_map<BatchUploadDataType, BatchUploadDataContainer>
                      data_containers,
                  SelectedDataTypeItemsCallback selected_items_callback);

 private:
  // Success callback of the dialog view, allows proceeding with the move of the
  // selected data items per data type to the account storages.
  void MoveItemsToAccountStorage(
      const base::flat_map<BatchUploadDataType,
                           std::vector<BatchUploadDataItemModel::DataId>>&
          items_to_move);

  SelectedDataTypeItemsCallback selected_items_callback_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_CONTROLLER_H_
