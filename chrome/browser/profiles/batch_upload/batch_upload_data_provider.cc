// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"

BatchUploadDataProvider::BatchUploadDataProvider(syncer::DataType type)
    : type_(type) {}

BatchUploadDataProvider::~BatchUploadDataProvider() = default;

syncer::DataType BatchUploadDataProvider::GetDataType() const {
  return type_;
}
