// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"

#include <array>
#include <map>

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
// An exception to the order of this list may happen to the first item, when the
// entry point used to open the batch upload dialog is tied to a data type. This
// data type will be shown first for consistency with the action from the entry
// point. E.g. Bookmarks promo card entry point will force
// `syncer::DataType::BOOKMARKS` to be the first data type shown (and not
// repeated afterwards).
const std::array<syncer::DataType, 5> kBatchUploadAvailableTypesOrder{
    // clang-format off
    syncer::DataType::PASSWORDS,
    syncer::DataType::BOOKMARKS,
    syncer::DataType::READING_LIST,
    syncer::DataType::CONTACT_INFO,
    syncer::DataType::THEMES,
    // clang-format on
};

// Determines the primary type based on the entry point.
// TODO(crbug.com/416219929): Future entry points may not be tied to a single
// data type, this would need to return std::nullopt then.
syncer::DataType PrimaryTypeFromEntryPoint(
    BatchUploadService::EntryPoint entry_point) {
  switch (entry_point) {
    case BatchUploadService::EntryPoint::kPasswordManagerSettings:
    case BatchUploadService::EntryPoint::kPasswordPromoCard:
      return syncer::PASSWORDS;
    case BatchUploadService::EntryPoint::kBookmarksManagerPromoCard:
      return syncer::BOOKMARKS;
  }
}

// Returns the list of data descriptions in `local_data_descriptions_map`
// following the expected displayed order in the dialog.
// `entry_point` is used to determine the primary (first) type to be displayed,
// overriding the expected order for the first type only. Primary type should
// have local data - since the entry point was available.
// Other data descriptions with no local data will be filtered out.
std::vector<syncer::LocalDataDescription>
GetOrderedListOfNonEmptyDataDescriptions(
    std::map<syncer::DataType, syncer::LocalDataDescription>
        local_data_descriptions_map,
    BatchUploadService::EntryPoint entry_point) {
  std::vector<syncer::LocalDataDescription> local_data_description_list;

  // Treat the primary type first to make sure it is the first type for display,
  // overriding the expected order given in `kBatchUploadAvailableTypesOrder`.
  syncer::DataType primary_type = PrimaryTypeFromEntryPoint(entry_point);
  CHECK(base::Contains(kBatchUploadAvailableTypesOrder, primary_type));
  CHECK(local_data_descriptions_map.contains(primary_type));
  syncer::LocalDataDescription& primary_local_data_description =
      local_data_descriptions_map[primary_type];
  CHECK(!primary_local_data_description.local_data_models.empty())
      << "Primary data type should have local data since the entry point "
         "is available.";
  CHECK_EQ(primary_type, primary_local_data_description.type)
      << "Non empty data description's data type and the keyed mapping "
         "value should always match.";
  local_data_description_list.push_back(
      std::move(primary_local_data_description));
  local_data_descriptions_map.erase(primary_type);

  // Reorder the result from the `local_data_descriptions_map` based on the
  // available types order.
  for (syncer::DataType type : kBatchUploadAvailableTypesOrder) {
    if (!local_data_descriptions_map.contains(type)) {
      continue;
    }

    syncer::LocalDataDescription& local_data_description =
        local_data_descriptions_map.at(type);
    if (!local_data_description.local_data_models.empty()) {
      CHECK_EQ(type, local_data_description.type)
          << "Non empty data description's data type and the keyed mapping "
             "value should always match.";
      local_data_description_list.push_back(std::move(local_data_description));
    }
    local_data_descriptions_map.erase(type);
  }

  // All initial inputs should be processed.
  CHECK(local_data_descriptions_map.empty());
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

  GetLocalDataDescriptionsForAvailableTypes(
      base::BindOnce(&BatchUploadService::OnGetLocalDataDescriptionsReady,
                     base::Unretained(this)));
}

void BatchUploadService::GetLocalDataDescriptionsForAvailableTypes(
    base::OnceCallback<
        void(std::map<syncer::DataType, syncer::LocalDataDescription>)>
        result_callback) {
  syncer::DataTypeSet data_types;
  // Iterate over all available enums.
  for (syncer::DataType type : kBatchUploadAvailableTypesOrder) {
    data_types.Put(type);
  }

  sync_service_->GetLocalDataDescriptions(data_types,
                                          std::move(result_callback));
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
      GetOrderedListOfNonEmptyDataDescriptions(
          std::move(local_data_map), state_.dialog_state_->entry_point_),
      state_.dialog_state_->entry_point_,
      /*complete_callback=*/
      base::BindOnce(&BatchUploadService::OnBatchUploadDialogResult,
                     base::Unretained(this)));
  std::move(state_.dialog_state_->dialog_shown_callback_).Run(true);
}

void BatchUploadService::OnBatchUploadDialogResult(
    const std::map<syncer::DataType,
                   std::vector<syncer::LocalDataItemModel::DataId>>&
        item_ids_to_move) {
  CHECK(state_.dialog_state_);

  Browser* browser = state_.dialog_state_->browser_.get();
  ResetDialogState();

  if (item_ids_to_move.empty()) {
    return;
  }

  sync_service_->TriggerLocalDataMigrationForItems(item_ids_to_move);

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
          ->SetExplicitButtonState(
              l10n_util::GetStringUTF16(
                  IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT),
              /*accessibility_label=*/std::nullopt,
              /*explicit_action=*/std::nullopt);
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

// static
std::vector<syncer::DataType>
BatchUploadService::AvailableTypesOrderForTesting() {
  // Transforming to vector to avoid changing every definition on updates.
  return base::ToVector(kBatchUploadAvailableTypesOrder);
}
