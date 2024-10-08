// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"

BatchUploadDataItemModel::BatchUploadDataItemModel() = default;
BatchUploadDataItemModel::~BatchUploadDataItemModel() = default;
BatchUploadDataItemModel::BatchUploadDataItemModel(
    BatchUploadDataItemModel&& other) = default;
BatchUploadDataItemModel& BatchUploadDataItemModel::operator=(
    BatchUploadDataItemModel&& other) = default;

BatchUploadDataContainer::BatchUploadDataContainer(int section_title_id,
                                                   int dialog_subtitle_id)
    : section_title_id(section_title_id),
      dialog_subtitle_id(dialog_subtitle_id) {}

BatchUploadDataContainer::BatchUploadDataContainer(
    BatchUploadDataContainer&& other) = default;

BatchUploadDataContainer& BatchUploadDataContainer::operator=(
    BatchUploadDataContainer&& other) = default;

BatchUploadDataContainer::~BatchUploadDataContainer() = default;

BatchUploadDataProvider::BatchUploadDataProvider(BatchUploadDataType type)
    : type_(type) {}

BatchUploadDataProvider::~BatchUploadDataProvider() = default;

BatchUploadDataType BatchUploadDataProvider::GetDataType() const {
  return type_;
}
