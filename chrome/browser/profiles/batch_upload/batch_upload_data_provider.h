// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_

#include <string>
#include <vector>

#include "components/sync/service/local_data_description.h"
#include "url/gurl.h"

// Interface to be implemented by each data type that needs to integrate with
// the Batch Upload to allow it's local data to be uploaded to the Account
// Storage through the Batch Upload dialog.
//
// TODO(crbug.com/372827385): Remove as it will be replaced with getting
// information directly from the SyncService. It is almost not used anymore
// execpt for getting Fake Local data and testing.
class BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProvider(syncer::DataType type);

  virtual ~BatchUploadDataProvider();

  syncer::DataType GetDataType() const;

  // Returns whether the data type has local data that are allowed to be
  // uploaded. This is a lightweight version of `GetLocalData()` that is not
  // expected to allocate memory to be used to perform early checks.
  virtual bool HasLocalData() const = 0;

  // Returns all the current local data of a specific data type, along with all
  // the information that needs to be displayed in the Batch Upload dialog.
  // In the data type is disabled or uploading local data is not allowed for the
  // type, the container should returned should be empty.
  // Empty container would not show any information for the data type.
  virtual syncer::LocalDataDescription GetLocalData() const = 0;

  // Given the list of items ids that were selected in the Batch Upload dialog,
  // performs the move to the account storage.
  virtual bool MoveToAccountStorage(
      const std::vector<syncer::LocalDataItemModel::DataId>&
          item_ids_to_move) = 0;

 private:
  // The type should always match when `this` is a value of a map keyed by
  // `BatchUploadDataType`.
  const syncer::DataType type_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_
