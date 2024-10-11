// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_

#include <string>
#include <vector>

#include "url/gurl.h"

enum class BatchUploadDataType;

// Representation of a single item to be displayed in the BatchUpload dialog.
struct BatchUploadDataItemModel {
  // This id corresponds to the data item being represented in the model. It
  // will be used to link back to the data accurately when needing to process
  // the result of the dialog.
  // Currently only needed as a string, but can potentially be extended to be a
  // `std::variant<IdType1, IdType2, ...>` based on the data id types that are
  // added.
  using DataId = std::string;

  // Reprensents the underlying data id.
  DataId id;

  // Icon url for the icon of the item model. If empty the the icon will be
  // hidden.
  GURL icon_url;

  // Used as the primary text of the item model.
  std::string title;

  // Used as the secondary text of the item model.
  std::string subtitle;

  // TODO(b/359150954): handle optional data logic -- e.g. passwords with reveal
  // callback, this may be handled in the controller/dialog directly.

  BatchUploadDataItemModel();
  ~BatchUploadDataItemModel();
  // Not copyable.
  BatchUploadDataItemModel(const BatchUploadDataItemModel&) = delete;
  BatchUploadDataItemModel& operator=(const BatchUploadDataItemModel&) = delete;
  // Movable.
  BatchUploadDataItemModel(BatchUploadDataItemModel&& other);
  BatchUploadDataItemModel& operator=(BatchUploadDataItemModel&& other);

  bool operator==(const BatchUploadDataItemModel& other) const;
};

// Container representing a data type section in the BatchUpload dialog.
struct BatchUploadDataContainer {
  BatchUploadDataType type;

  // Used as the section title message id for the name in the Batch Upload
  // dialog. Along with its string representation as the message name, it will
  // be used to compute the section title in WebUi. The generated string in
  // WebUi supports Plural casing with an integer param.
  int section_title_id;

  // List of items to be displayed in a specific section of a data type in the
  // Batch Upload dialog.
  std::vector<BatchUploadDataItemModel> items;

  explicit BatchUploadDataContainer(BatchUploadDataType type,
                                    int section_name_id);
  ~BatchUploadDataContainer();
  // Not copyable.
  BatchUploadDataContainer(const BatchUploadDataContainer&) = delete;
  BatchUploadDataContainer& operator=(const BatchUploadDataContainer&) = delete;
  // Movable.
  BatchUploadDataContainer(BatchUploadDataContainer&& other);
  BatchUploadDataContainer& operator=(BatchUploadDataContainer&& other);

  bool operator==(const BatchUploadDataContainer& other) const;
};

// Interface to be implemented by each data type that needs to integrate with
// the Batch Upload to allow it's local data to be uploaded to the Account
// Storage through the Batch Upload dialog.
//
// TODO(crbug.com/372827385): Remove as it will be replaced with getting
// information directly from the SyncService. It is almost not used anymore
// execpt for getting Fake Local data and testing.
class BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProvider(BatchUploadDataType type);

  virtual ~BatchUploadDataProvider();

  BatchUploadDataType GetDataType() const;

  // Returns whether the data type has local data that are allowed to be
  // uploaded. This is a lightweight version of `GetLocalData()` that is not
  // expected to allocate memory to be used to perform early checks.
  virtual bool HasLocalData() const = 0;

  // Returns all the current local data of a specific data type, along with all
  // the information that needs to be displayed in the Batch Upload dialog.
  // In the data type is disabled or uploading local data is not allowed for the
  // type, the container should returned should be empty.
  // Empty container would not show any information for the data type.
  virtual BatchUploadDataContainer GetLocalData() const = 0;

  // Given the list of items ids that were selected in the Batch Upload dialog,
  // performs the move to the account storage.
  virtual bool MoveToAccountStorage(
      const std::vector<BatchUploadDataItemModel::DataId>&
          item_ids_to_move) = 0;

 private:
  // The type should always match when `this` is a value of a map keyed by
  // `BatchUploadDataType`.
  const BatchUploadDataType type_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_
