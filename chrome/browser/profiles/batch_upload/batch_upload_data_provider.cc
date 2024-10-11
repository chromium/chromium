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

bool BatchUploadDataItemModel::operator==(
    const BatchUploadDataItemModel& other) const = default;

BatchUploadDataContainer::BatchUploadDataContainer(BatchUploadDataType type,
                                                   int section_title_id)
    : type(type), section_title_id(section_title_id) {}
BatchUploadDataContainer::~BatchUploadDataContainer() = default;

BatchUploadDataContainer::BatchUploadDataContainer(
    BatchUploadDataContainer&& other) = default;
BatchUploadDataContainer& BatchUploadDataContainer::operator=(
    BatchUploadDataContainer&& other) = default;

bool BatchUploadDataContainer::operator==(
    const BatchUploadDataContainer& other) const = default;

BatchUploadDataProvider::BatchUploadDataProvider(BatchUploadDataType type)
    : type_(type) {}

BatchUploadDataProvider::~BatchUploadDataProvider() = default;

BatchUploadDataType BatchUploadDataProvider::GetDataType() const {
  return type_;
}
