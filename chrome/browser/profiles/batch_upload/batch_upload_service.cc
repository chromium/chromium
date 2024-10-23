// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"

#include <array>
#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Duration of displaying the saving to account text in the avatar button.
constexpr base::TimeDelta kBatchUploadAvatarButtonOverrideTextDuration =
    base::Seconds(3);

// This list contains all the data types that are available for the Batch Upload
// dialog. Data types should not be repeated and the list is ordered based on
// the priority of showing in the dialog.
const std::array<syncer::DataType, 2> kBatchUploadOrderedAvailableTypes{
    syncer::DataType::PASSWORDS,
    syncer::DataType::CONTACT_INFO,
};

}  // namespace

BatchUploadService::BatchUploadService(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    std::unique_ptr<BatchUploadDelegate> delegate)
    : identity_manager_(*identity_manager),
      sync_service_(*sync_service),
      delegate_(std::move(delegate)) {}

BatchUploadService::~BatchUploadService() = default;

void BatchUploadService::OpenBatchUpload(
    Browser* browser,
    base::OnceCallback<void(bool)> success_callback) {
  if (!IsUserEligibleToOpenDialog()) {
    std::move(success_callback).Run(false);
    return;
  }

  // Do not allow to have more than one controller/dialog shown at a time.
  if (IsDialogOpened()) {
    // TODO(b/361330952): give focus to the browser that is showing the dialog
    // currently.
    std::move(success_callback).Run(false);
    return;
  }

  // Create the controller in preparation for showing the dialog once all the
  // local data descriptions are ready in `OnLocalDataDescriptionsReady()`.
  // Allows to make sure that while getting the local data descriptions, no
  // other dialog opening is triggered.
  controller_ = std::make_unique<BatchUploadController>();
  browser_ = browser;
  dialog_shown_callback_ = std::move(success_callback);

  RequestLocalDataDescriptions();
}

void BatchUploadService::RequestLocalDataDescriptions() {
  syncer::DataTypeSet data_types;
  // Iterate over all available enums.
  for (syncer::DataType type : kBatchUploadOrderedAvailableTypes) {
    data_types.Put(type);
  }

  sync_service_->GetLocalDataDescriptions(
      data_types,
      base::BindOnce(&BatchUploadService::OnGetLocalDataDescriptionsReady,
                     base::Unretained(this)));
}

void BatchUploadService::OnGetLocalDataDescriptionsReady(
    std::map<syncer::DataType, syncer::LocalDataDescription> local_data_map) {
  if (local_data_map.empty()) {
    Reset();
    std::move(dialog_shown_callback_).Run(false);
    return;
  }

  bool opened = controller_->ShowDialog(
      *delegate_, browser_,
      std::move(local_data_map), /*selected_items_callback=*/
      base::BindOnce(&BatchUploadService::OnBatchUplaodDialogResult,
                     base::Unretained(this)));
  std::move(dialog_shown_callback_).Run(opened);
}

void BatchUploadService::OnBatchUplaodDialogResult(
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>&
        item_ids_to_move) {
  CHECK(controller_);

  Browser* browser = browser_.get();
  Reset();

  if (item_ids_to_move.empty()) {
    return;
  }

  sync_service_->TriggerLocalDataMigration(item_ids_to_move);

  TriggerAvatarButtonSavingDataText(browser);
}

bool BatchUploadService::IsUserEligibleToOpenDialog() const {
  AccountInfo primary_account = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  // If not signed in or syncing, the user should not have access to the dialog.
  if (primary_account.IsEmpty() ||
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  // If is in Sign in pending, the user should not have access to the dialog.
  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account.account_id)) {
    return false;
  }

  return true;
}

void BatchUploadService::TriggerAvatarButtonSavingDataText(Browser* browser) {
  CHECK(browser);
  // Show the text.
  avatar_override_clear_callback_ =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetAvatarToolbarButton()
          ->ShowExplicitText(
              l10n_util::GetStringUTF16(
                  IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT),
              std::nullopt);
  // Prepare the timer to stop the overridden text from showing.
  avatar_override_timer_.Start(
      FROM_HERE, kBatchUploadAvatarButtonOverrideTextDuration,
      base::BindOnce(&BatchUploadService::OnAvatarOverrideTextTimeout,
                     // Unretained is fine here since the timer is a field
                     // member and will not fire if destroyed.
                     base::Unretained(this)));
}

void BatchUploadService::OnAvatarOverrideTextTimeout() {
  CHECK(avatar_override_clear_callback_);
  avatar_override_clear_callback_.RunAndReset();
}

bool BatchUploadService::IsDialogOpened() const {
  return controller_ != nullptr;
}

void BatchUploadService::Reset() {
  controller_.reset();
  browser_ = nullptr;
}
