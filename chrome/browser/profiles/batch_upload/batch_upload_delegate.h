// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DELEGATE_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"

enum class BatchUploadDataType;
class Browser;

using SelectedDataTypeItemsCallback = base::OnceCallback<void(
    const base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::Id>>&)>;

// Delegate responsible of showing the Batch Upload Dialog view.
class BatchUploadDelegate {
 public:
  virtual ~BatchUploadDelegate() = default;

  // If `complete_callback` is called with no elements in the map, then the move
  // request was cancelled.
  virtual void ShowBatchUploadDialog(
      Browser* browser,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_DELEGATE_H_
