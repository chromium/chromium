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

// Data descriptions with no local data will be filtered out.
std::vector<syncer::LocalDataDescription>
GetOrderedListOfNonEmptyDataDescriptions(
    std::map<syncer::DataType, syncer::LocalDataDescription>
        local_data_descriptions_map) {
  // TODO(crbug.com/361340640): make the data type entry point the first one.
  // TODO(crbug.com/374133537): Use `kBatchUploadOrderedAvailableTypes` types
  // order to reorder the returned list for display order.
  std::vector<syncer::LocalDataDescription> local_data_description_list;
  for (auto& [type, local_data_description] : local_data_descriptions_map) {
    if (!local_data_description.local_data_models.empty()) {
      CHECK_EQ(type, local_data_description.type)
          << "Non empty data description's data type and the keyed mapping "
             "value should always match.";

      local_data_description_list.push_back(std::move(local_data_description));
    }
  }
  return local_data_description_list;
}

// Whether there exist a current local data item of any type.
bool HasLocalDataToShow(
    const std::map<syncer::DataType, syncer::LocalDataDescription>&
        local_data_descriptions) {
  // As long as a data type has at least a single item to show, the dialog can
  // be shown.
  return std::ranges::any_of(
      local_data_descriptions, [](const auto& local_data_description) {
        return !local_data_description.second.local_data_models.empty();
      });
}

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
    EntryPoint entry_point,
    base::OnceCallback<void(bool)> success_callback) {
  if (!IsUserEligibleToOpenDialog()) {
    std::move(success_callback).Run(false);
    return;
  }

  // Do not allow to have more than one dialog shown at a time.
  if (IsDialogOpened()) {
    // TODO(b/361330952): give focus to the browser that is showing the dialog
    // currently.
    std::move(success_callback).Run(false);
    return;
  }

  // Create the state of the dialog that may be shown, in preparation for
  // showing the dialog once all the local data descriptions are ready in
  // `OnGetLocalDataDescriptionsReady()`. Allows to make sure that while getting
  // the local data descriptions, no other dialog opening is triggered.
  state_.dialog_state_ = std::make_unique<ResettableState::DialogState>();
  state_.dialog_state_->browser_ = browser;
  state_.dialog_state_->entry_point_ = entry_point;
  state_.dialog_state_->dialog_shown_callback_ = std::move(success_callback);

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
  if (local_data_map.empty() || !HasLocalDataToShow(local_data_map)) {
    std::move(state_.dialog_state_->dialog_shown_callback_).Run(false);
    ResetDialogState();
    return;
  }

  delegate_->ShowBatchUploadDialog(
      state_.dialog_state_->browser_,
      GetOrderedListOfNonEmptyDataDescriptions(std::move(local_data_map)),
      state_.dialog_state_->entry_point_,
      /*complete_callback=*/
      base::BindOnce(&BatchUploadService::OnBatchUplaodDialogResult,
                     base::Unretained(this)));
  std::move(state_.dialog_state_->dialog_shown_callback_).Run(true);
}

void BatchUploadService::OnBatchUplaodDialogResult(
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>&
        item_ids_to_move) {
  CHECK(state_.dialog_state_);

  Browser* browser = state_.dialog_state_->browser_.get();
  ResetDialogState();

  if (item_ids_to_move.empty()) {
    return;
  }

  sync_service_->TriggerLocalDataMigration(item_ids_to_move);

  // `browser` may be null in tests.
  if (browser) {
    state_.saving_browser_state_ =
        std::make_unique<ResettableState::SavingBrowserState>();
    TriggerAvatarButtonSavingDataText(browser);
  }
}

bool BatchUploadService::IsUserEligibleToOpenDialog() const {
  AccountInfo primary_account = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  // If not signed in, the user should not have access to the dialog.
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
  CHECK(state_.saving_browser_state_);
  // Show the text.
  state_.saving_browser_state_->avatar_override_clear_callback_ =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetAvatarToolbarButton()
          ->ShowExplicitText(
              l10n_util::GetStringUTF16(
                  IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT),
              std::nullopt);
  // Prepare the timer to stop the overridden text from showing.
  state_.saving_browser_state_->avatar_override_timer_.Start(
      FROM_HERE, kBatchUploadAvatarButtonOverrideTextDuration,
      base::BindOnce(&BatchUploadService::OnAvatarOverrideTextTimeout,
                     // Unretained is fine here since the timer is a field
                     // member and will not fire if destroyed.
                     base::Unretained(this)));
}

void BatchUploadService::OnAvatarOverrideTextTimeout() {
  CHECK(state_.saving_browser_state_ &&
        state_.saving_browser_state_->avatar_override_clear_callback_);
  state_.saving_browser_state_->avatar_override_clear_callback_.RunAndReset();
  state_.saving_browser_state_.reset();
}

bool BatchUploadService::IsDialogOpened() const {
  return state_.dialog_state_ != nullptr;
}

void BatchUploadService::ResetDialogState() {
  state_.dialog_state_.reset();
}

BatchUploadService::ResettableState::ResettableState() = default;
BatchUploadService::ResettableState::~ResettableState() = default;

BatchUploadService::ResettableState::DialogState::DialogState() = default;
BatchUploadService::ResettableState::DialogState::~DialogState() = default;
