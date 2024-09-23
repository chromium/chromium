// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class Profile;
enum class BatchUploadDataType;
class BatchUploadController;
class BatchUploadDelegate;

// Service tied to a profile that allows the management of the Batch Upload
// Dialog. It communicates with the different data type services that needs to
// integerate with the Batch Upload service.
// Used to open the dialog and manages the lifetime of the controller.
class BatchUploadService : public KeyedService {
 public:
  explicit BatchUploadService(Profile& profile,
                              std::unique_ptr<BatchUploadDelegate> delegate);
  BatchUploadService(const BatchUploadService&) = delete;
  BatchUploadService& operator=(const BatchUploadService&) = delete;
  ~BatchUploadService() override;

  // Attempts to open the Batch Upload modal dialog that allows uploading the
  // local profile data. The dialog will only be opened if there are some local
  // data (of any type) to show and the dialog is not shown already in the
  // profile. Retrurns whether the dialog was shown or not.
  bool OpenBatchUpload(Browser* browser);

  // Allows to know if a specific data type should have its BatchUpload entry
  // point (access to the Batch Upload dialog) displayed. This performs the
  // check on the specific requested type, and not the rest of the available
  // types, meaning that if other types have local data to be displayed but not
  // the requested one, the entry point should not be shown.
  bool ShouldShowBatchUploadEntryPointForDataType(BatchUploadDataType type);

 private:
  // Callback on dialog closed. The `move_requested` input determines whether
  // the dialog was closed with a Cancel/Upload request.
  void OnBatchUplaodDialogClosed(bool move_requested);

  base::raw_ref<Profile> profile_;
  std::unique_ptr<BatchUploadDelegate> delegate_;

  // Controller lifetime is bind to when the dialog is currently showing. There
  // can only be one controller/dialog existing at the same time per profile.
  std::unique_ptr<BatchUploadController> controller_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_
