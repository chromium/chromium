// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
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
  // profile. `dialog_shown_callback` returns whether the dialog was shown or
  // not.
  void OpenBatchUpload(
      Browser* browser,
      base::OnceCallback<void(bool)> dialog_shown_callback = base::DoNothing());

  // Returns whether the dialog is currently showing on a browser.
  bool IsDialogOpened() const;

 private:
  // Iterates over all available types that can be displayed in the dialog and
  // request the container. Result of each containers is returned in
  // `OnBatchUploadContainerReady()` asynchronously.
  void RequestBatchUploadDataContainers();

  // Barrier callback aggregating the `BatchUploadDataContainer`s. It is
  // expected to return a container per data type in `BatchUploadDataType` even
  // if the returned container is empty. Once all the containers are available,
  // triggers showing the dialog.
  void OnBatchUploadContainersReady(
      std::vector<BatchUploadDataContainer> data_containers);

  // Callback of the dialog view closing, contains the IDs of the selected items
  // per data type. Selected items will be processed to be moved to the account
  // storage. Empty map means the dialog was closed explicitly not to move any
  // data.
  void OnBatchUplaodDialogResult(
      const base::flat_map<BatchUploadDataType,
                           std::vector<BatchUploadDataItemModel::DataId>>&
          item_ids_to_move);

  // Whether the profile is in the proper sign in state to see the dialog.
  bool IsUserEligibleToOpenDialog() const;

  // Changes the avatar button text to saving data and starts a timer that will
  // revert the button text on timeout.
  void TriggerAvatarButtonSavingDataText(Browser* browser);

  // Callback to clear the overridden avatar text on timeout.
  void OnAvatarOverrideTextTimeout();

  base::raw_ref<Profile> profile_;
  std::unique_ptr<BatchUploadDelegate> delegate_;

  // Controller lifetime is bind to when the dialog is currently showing. There
  // can only be one controller/dialog existing at the same time per profile.
  std::unique_ptr<BatchUploadController> controller_;
  // Browser that is showing the dialog. Nullptr if the dialog is not opened.
  raw_ptr<Browser> browser_;
  // When accepting the bubble, the avatar button text is modified and this
  // callback handles it's lifetime. Executing it will clear the text.
  base::ScopedClosureRunner avatar_override_clear_callback_;
  // Timer to clear the avatar override text. Triggered after accepting the
  // bubble.
  base::OneShotTimer avatar_override_timer_;

  // Called when the decision about showing the dialog is made.
  // Returns whether it was shown or not.
  base::OnceCallback<void(bool)> dialog_shown_callback_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_
