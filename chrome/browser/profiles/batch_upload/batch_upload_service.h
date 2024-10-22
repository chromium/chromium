// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/service/local_data_description.h"

class Browser;
class BatchUploadController;
class BatchUploadDelegate;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Service that allows the management of the Batch Upload Dialog. Used to open
// the dialog and manages the lifetime of the controller.
// It communicates with the `sync_service` to get information of the current
// local data for eligible types.
class BatchUploadService : public KeyedService {
 public:
  BatchUploadService(signin::IdentityManager* identity_manager,
                     syncer::SyncService* sync_service,
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
  // request the `syncer::LocalDataDescription that contains the list of items.
  // The result is returned asynchronously.
  void RequestLocalDataDescriptions();

  // Callback that returns a map of `syncer::LocalDataDescription` for the data
  // types that can be shown in the Batch Upload dialog.
  void OnGetLocalDataDescriptionsReady(
      std::map<syncer::DataType, syncer::LocalDataDescription> local_data_map);

  // Callback of the dialog view closing, contains the IDs of the selected items
  // per data type. Selected items will be processed to be moved to the account
  // storage. Empty map means the dialog was closed explicitly not to move any
  // data.
  void OnBatchUplaodDialogResult(
      const std::map<syncer::DataType,
                     std::vector<syncer::LocalDataItemModel::DataId>>&
          item_ids_to_move);

  // Whether the profile is in the proper sign in state to see the dialog.
  bool IsUserEligibleToOpenDialog() const;

  // Changes the avatar button text to saving data and starts a timer that will
  // revert the button text on timeout.
  void TriggerAvatarButtonSavingDataText(Browser* browser);

  // Callback to clear the overridden avatar text on timeout.
  void OnAvatarOverrideTextTimeout();

  // Resets the state of the service related to the dialog.
  void Reset();

  raw_ref<signin::IdentityManager> identity_manager_;
  raw_ref<syncer::SyncService> sync_service_;
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
