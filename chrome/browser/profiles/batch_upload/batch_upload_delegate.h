// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DELEGATE_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "components/sync/service/local_data_description.h"

class Browser;

using BatchUploadSelectedDataTypeItemsCallback = base::OnceCallback<void(
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>&)>;

// Delegate responsible of showing the Batch Upload Dialog view.
class BatchUploadDelegate {
 public:
  virtual ~BatchUploadDelegate() = default;

  // If `complete_callback` is called with no elements in the map, then the move
  // request was cancelled.
  virtual void ShowBatchUploadDialog(
      Browser* browser,
      std::vector<syncer::LocalDataDescription> local_data_descriptions_list,
      BatchUploadService::EntryPoint entry_point,
      BatchUploadSelectedDataTypeItemsCallback complete_callback) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DELEGATE_H_
