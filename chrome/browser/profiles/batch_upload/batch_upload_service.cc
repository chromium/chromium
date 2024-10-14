// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
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
#include "ui/base/l10n/l10n_util.h"

namespace {

// Duration of displaying the saving to account text in the avatar button.
constexpr base::TimeDelta kBatchUploadAvatarButtonOverrideTextDuration =
    base::Seconds(3);

// Returns a dummy implementation of container through `on_container_ready`.
// TODO(b/359146556): remove when actual providers are implemented.
void MakeDummyBatchUploadDataContainerGetter(
    BatchUploadDataType type,
    int title_id,
    int item_count,
    base::OnceCallback<void(BatchUploadDataContainer)> on_container_ready) {
  BatchUploadDataContainer container(type,
                                     /*section_name_id=*/title_id);
  for (int i = 0; i < item_count; ++i) {
    BatchUploadDataItemModel item;
    item.id = BatchUploadDataItemModel::DataId(base::ToString(i));
    item.icon_url = type == BatchUploadDataType::kPasswords
                        ? GURL("chrome://theme/IDR_PASSWORD_MANAGER_FAVICON")
                        : GURL();
    item.title = "title_" + base::UTF16ToUTF8(base::FormatNumber(i));
    item.subtitle = "subtitle_" + base::UTF16ToUTF8(base::FormatNumber(i));
    container.items.push_back(std::move(item));
  }
  std::move(on_container_ready).Run(std::move(container));
}

// Triggers the asynchronous request of `BatchUploadDataContainer` for `type`.
// `on_container_ready` should always return a value even if empty.
void RequestBatchUploadDataContainer(
    Profile& profile,
    BatchUploadDataType type,
    base::OnceCallback<void(BatchUploadDataContainer)> on_container_ready) {
  // TODO(crbug.com/359146556): real implementations to be added per data type
  // and linked to the sync service underlying type.
  switch (type) {
    case BatchUploadDataType::kPasswords:
      MakeDummyBatchUploadDataContainerGetter(
          type, IDS_BATCH_UPLOAD_SECTION_TITLE_PASSWORDS, 2,
          std::move(on_container_ready));
      break;
    case BatchUploadDataType::kAddresses:
      MakeDummyBatchUploadDataContainerGetter(
          type, IDS_BATCH_UPLOAD_SECTION_TITLE_ADDRESSES, 20,
          std::move(on_container_ready));
      break;
  }
}

}  // namespace

BatchUploadService::BatchUploadService(
    Profile& profile,
    std::unique_ptr<BatchUploadDelegate> delegate)
    : profile_(profile), delegate_(std::move(delegate)) {}

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
  // containers are ready in `OnBatchUploadContainerReady()`. Allows to make
  // sure that while getting the containers, no other dialog opening is
  // triggered.
  controller_ = std::make_unique<BatchUploadController>();
  browser_ = browser;
  dialog_shown_callback_ = std::move(success_callback);

  RequestBatchUploadDataContainers();
}

void BatchUploadService::RequestBatchUploadDataContainers() {
  size_t total_data_type_count =
      static_cast<size_t>(BatchUploadDataType::kMaxValue) + 1;
  auto barrier_callback = base::BarrierCallback<BatchUploadDataContainer>(
      total_data_type_count,
      base::BindOnce(&BatchUploadService::OnBatchUploadContainersReady,
                     base::Unretained(this)));

  // Iterate over all available enums. Should not use
  // `requested_data_types_remaining_count_` to iterate as it may be changed
  // during the loop execution if the callback is called synchronously.
  for (size_t enum_index = 0; enum_index < total_data_type_count;
       ++enum_index) {
    RequestBatchUploadDataContainer(
        profile_.get(), static_cast<BatchUploadDataType>(enum_index),
        barrier_callback);
  }
}

void BatchUploadService::OnBatchUploadContainersReady(
    std::vector<BatchUploadDataContainer> data_containers) {
  base::flat_map<BatchUploadDataType, BatchUploadDataContainer>
      data_containers_map;
  for (BatchUploadDataContainer& container : data_containers) {
    BatchUploadDataType type = container.type;
    data_containers_map.insert_or_assign(type, std::move(container));
  }

  bool opened = controller_->ShowDialog(
      *delegate_, browser_,
      std::move(data_containers_map), /*selected_items_callback=*/
      base::BindOnce(&BatchUploadService::OnBatchUplaodDialogResult,
                     base::Unretained(this)));
  std::move(dialog_shown_callback_).Run(opened);
}

void BatchUploadService::OnBatchUplaodDialogResult(
    const base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::DataId>>&
        item_ids_to_move) {
  CHECK(controller_);

  Browser* browser = browser_.get();
  // Reset the state of the service.
  controller_.reset();
  browser_ = nullptr;

  if (item_ids_to_move.empty()) {
    return;
  }

  // TODO(crbug.com/372701325): Process `item_ids_to_move` with SyncService.
  for (const auto& [type, item_id_list] : item_ids_to_move) {
    LOG(ERROR) << "XXX: Moving items: " << static_cast<int>(type);
    for (auto& id : item_id_list) {
      LOG(ERROR) << "XXX: id: " << id;
    }
  }

  TriggerAvatarButtonSavingDataText(browser);
}

bool BatchUploadService::IsUserEligibleToOpenDialog() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile_.get());

  AccountInfo primary_account = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  // If not signed in, the user should not have access to the dialog.
  if (primary_account.IsEmpty()) {
    return false;
  }

  // If is in Sign in pending, the user should not have access to the dialog.
  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
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
