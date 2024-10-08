// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_

#include <string>
#include <vector>

#include "base/types/id_type.h"
#include "url/gurl.h"

enum class BatchUploadDataType;

// Representation of a single item to be displayed in the BatchUpload dialog.
struct BatchUploadDataItemModel {
  BatchUploadDataItemModel();
  ~BatchUploadDataItemModel();
  BatchUploadDataItemModel(BatchUploadDataItemModel&& other);
  BatchUploadDataItemModel& operator=(BatchUploadDataItemModel&& other);

  // Strong Alias ID which is reprenseted as an int.
  using Id = base::IdType32<BatchUploadDataItemModel>;

  // This field is used to map the model item to the actual underlying item
  // provided by the `LocalDataProvider`. Each instance of
  // `LocalDataProvider` needs to guarantee that the id mapping stays valid,
  // expecting that it will be returned through
  // `LocalDataProvider::MoveToAccountStorage()` later and finding a match.
  // A simple way would be to use the `id` as the index of the item in the
  // `LocalDataContainer::items` vector as long as the returned vector is
  // not modified while the dialog is shown.
  // TODO(b/359509890): Make the ID field more easily manageable.
  Id id;

  // Icon url for the icon of the item model. If empty the the icon will be
  // hidden.
  GURL icon_url;

  // Used as the primary text of the item model.
  std::string title;

  // Used as the secondary text of the item model.
  std::string subtitle;

  // TODO(b/359150954): handle optional data logic -- e.g. passwords with reveal
  // callback, this may be handled in the controller/dialog directly.
};

// Container representing a data type section in the BatchUpload dialog.
struct BatchUploadDataContainer {
  // Used as the section title message id for the name in the Batch Upload
  // dialog.
  int section_title_id;

  // Message id used as part of the text in the Batch Upload dialog for the main
  // (first) section displayed. The text may be plural, depending on the number
  // of elements in `items`.
  int dialog_subtitle_id;

  // List of items to be displayed in a specific section of a data type in the
  // Batch Upload dialog.
  std::vector<BatchUploadDataItemModel> items;

  BatchUploadDataContainer(int section_name_id, int dialog_subtitle_id);
  // Not copyable.
  BatchUploadDataContainer(const BatchUploadDataContainer&) = delete;
  BatchUploadDataContainer& operator=(const BatchUploadDataContainer&) = delete;
  // Movable.
  BatchUploadDataContainer(BatchUploadDataContainer&& other);
  BatchUploadDataContainer& operator=(BatchUploadDataContainer&& other);

  ~BatchUploadDataContainer();
};

// Interface to be implemented by each data type that needs to integrate with
// the Batch Upload to allow it's local data to be uploaded to the Account
// Storage through the Batch Upload dialog.
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
      const std::vector<BatchUploadDataItemModel::Id>& item_ids_to_move) = 0;

 private:
  // The type should always match when `this` is a value of a map keyed by
  // `BatchUploadDataType`.
  const BatchUploadDataType type_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DATA_PROVIDER_H_
