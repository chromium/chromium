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
class BatchUploadDelegate;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Service that allows the management of the Batch Upload Dialog. Used to open
// the dialog and manages its lifetime.
// It communicates with the `syncer::SyncService` to get information of the
// current local data for eligible types.
class BatchUploadService : public KeyedService {
 public:
  BatchUploadService(signin::IdentityManager* identity_manager,
                     syncer::SyncService* sync_service,
                     std::unique_ptr<BatchUploadDelegate> delegate);
  BatchUploadService(const BatchUploadService&) = delete;
  BatchUploadService& operator=(const BatchUploadService&) = delete;
  ~BatchUploadService() override;

  // Lists the different entry points to the Batch Upload Dialog.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(EntryPoint)
  enum class EntryPoint {
    kPasswordManagerSettings = 0,
    kPasswordPromoCard = 1,

    kMaxValue = kPasswordPromoCard,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:BatchUploadEntryPoint)

  // Attempts to open the Batch Upload modal dialog that allows uploading the
  // local profile data. The dialog will only be opened if there are some local
  // data (of any type) to show and the dialog is not shown already in the
  // profile. `dialog_shown_callback` returns whether the dialog was shown or
  // not.
  void OpenBatchUpload(
      Browser* browser,
      EntryPoint entry_point,
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

  // Resets part of the state related to the dialog lifetime.
  void ResetDialogState();

  // This state is divided into two parts:
  // - `DialogState`: that is active from triggering the request of opening the
  // dialog until the dialog is closed or resulted in not opening.
  // - `SavingBrowserState`: that is active after the dialog was accepted.
  // Accepting the dialog closes it and resets `DialogState`. However
  // post-accepting (saving), the avatar button of the browser that was showing
  // the dialog expands and shows a saving text for few seconds. This state
  // holds needed information for that modification. The dialog is still allowed
  // to be opened while this state is active.
  struct ResettableState {
    // Fields related to the dialog currently showing.
    struct DialogState {
      // Browser that is showing the dialog.
      raw_ptr<Browser> browser_;
      // Entry point of the dialog.
      EntryPoint entry_point_ = EntryPoint::kPasswordManagerSettings;
      // Called when the decision about showing the dialog is made.
      // Returns whether it was shown or not.
      base::OnceCallback<void(bool)> dialog_shown_callback_;

      DialogState();
      ~DialogState();
    };

    // Fields related to the effect on the browser post accepting the dialog.
    struct SavingBrowserState {
      // Callback that will clear the modified text on the avatar button.
      base::ScopedClosureRunner avatar_override_clear_callback_;
      // Timer to clear the avatar override text.
      base::OneShotTimer avatar_override_timer_;
    };

    std::unique_ptr<DialogState> dialog_state_;
    std::unique_ptr<SavingBrowserState> saving_browser_state_;

    ResettableState();
    ~ResettableState();
  };

  raw_ref<signin::IdentityManager> identity_manager_;
  raw_ref<syncer::SyncService> sync_service_;
  std::unique_ptr<BatchUploadDelegate> delegate_;

  // Full state of the flow from requesting opening the dialog to saving
  // data/canceling the flow.
  ResettableState state_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_H_
