// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"

#include "base/functional/bind.h"
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

// Temporary Dummy implementation. All IDs provided are arbitrary.
// TODO(b/359146556): remove when actual providers are implemented.
class DummyBatchUploadDataProvider : public BatchUploadDataProvider {
 public:
  explicit DummyBatchUploadDataProvider(BatchUploadDataType type,
                                        int title_id,
                                        int item_count)
      : BatchUploadDataProvider(type),
        title_id_(title_id),
        item_count_(item_count) {}

  bool HasLocalData() const override { return item_count_ > 0; }

  BatchUploadDataContainer GetLocalData() const override {
    BatchUploadDataContainer container(/*section_name_id=*/title_id_,
                                       /*dialog_subtitle_id=*/456);
    for (int i = 0; i < item_count_; ++i) {
      BatchUploadDataItemModel item;
      item.id = BatchUploadDataItemModel::Id(i);
      item.icon_url = GetDataType() == BatchUploadDataType::kPasswords
                          ? GURL("chrome://theme/IDR_PASSWORD_MANAGER_FAVICON")
                          : GURL();
      item.title = "title_" + base::UTF16ToUTF8(base::FormatNumber(i));
      item.subtitle = "subtitle_" + base::UTF16ToUTF8(base::FormatNumber(i));
      container.items.push_back(std::move(item));
    }
    return container;
  }

  bool MoveToAccountStorage(const std::vector<BatchUploadDataItemModel::Id>&
                                item_ids_to_move) override {
    // TODO(b/359146556): temporary output until there is the real
    // implementations.
    LOG(ERROR) << "XXX: Moving items:";
    for (auto& id : item_ids_to_move) {
      LOG(ERROR) << "XXX: id: " << id;
    }
    return true;
  }

 private:
  int title_id_ = 0;
  int item_count_ = 0;
};

// Returns a dummy implementation.
// TODO(b/359146556): remove when actual providers are implemented.
std::unique_ptr<BatchUploadDataProvider> MakeDummyBatchUploadDataProvider(
    BatchUploadDataType type,
    int title_id,
    int item_count) {
  return std::make_unique<DummyBatchUploadDataProvider>(type, title_id,
                                                        item_count);
}

// Gets the `BatchUploadDataProvider` of a single data type. Can also be used in
// order to know if a specific data type entry point for the BatchUpload should
// be visible or not, without needing to create the whole BatchUpload logic.
// The returned `BatchUploadDataProvider` should not be null.
std::unique_ptr<BatchUploadDataProvider> GetBatchUploadDataProvider(
    Profile& profile,
    BatchUploadDataType type) {
  // TODO(b/359146556): real implementations to be added per data type.
  switch (type) {
    case BatchUploadDataType::kPasswords:
      return MakeDummyBatchUploadDataProvider(
          type, IDS_BATCH_UPLOAD_SECTION_TITLE_PASSWORDS, 2);
    case BatchUploadDataType::kAddresses:
      return MakeDummyBatchUploadDataProvider(
          type, IDS_BATCH_UPLOAD_SECTION_TITLE_ADDRESSES, 20);
  }
}

// Helper function to get the map of all `BatchUploadDataProvider` of all data
// types that can have local data that can be displayed by the BatchUpload
// dialog.
base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
GetBatchUploadDataProviderMap(Profile& profile) {
  base::flat_map<BatchUploadDataType, std::unique_ptr<BatchUploadDataProvider>>
      data_providers;

  data_providers[BatchUploadDataType::kPasswords] =
      GetBatchUploadDataProvider(profile, BatchUploadDataType::kPasswords);
  data_providers[BatchUploadDataType::kAddresses] =
      GetBatchUploadDataProvider(profile, BatchUploadDataType::kAddresses);

  return data_providers;
}

}  // namespace

BatchUploadService::BatchUploadService(
    Profile& profile,
    std::unique_ptr<BatchUploadDelegate> delegate)
    : profile_(profile), delegate_(std::move(delegate)) {}

BatchUploadService::~BatchUploadService() = default;

bool BatchUploadService::OpenBatchUpload(Browser* browser) {
  if (!IsUserEligibleToOpenDialog()) {
    return false;
  }

  // Do not allow to have more than one controller/dialog shown at a time.
  if (IsDialogOpened()) {
    // TODO(b/361330952): give focus to the browser that is showing the dialog
    // currently.
    return false;
  }

  // Create the controller with all the implementations of available local data
  // providers.
  controller_ = std::make_unique<BatchUploadController>(
      GetBatchUploadDataProviderMap(profile_.get()));
  browser_ = browser;

  return controller_->ShowDialog(
      *delegate_, browser, /*done_callback=*/
      base::BindOnce(&BatchUploadService::OnBatchUplaodDialogClosed,
                     base::Unretained(this)));
}

void BatchUploadService::OnBatchUplaodDialogClosed(bool move_requested) {
  CHECK(controller_);
  if (move_requested) {
    avatar_override_clear_callback_ =
        BrowserView::GetBrowserViewForBrowser(browser_.get())
            ->toolbar_button_provider()
            ->GetAvatarToolbarButton()
            ->ShowExplicitText(
                l10n_util::GetStringUTF16(
                    IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT),
                // TODO(b/367938326): Add the right accessibility string.
                std::nullopt);
    avatar_override_timer_.Start(
        FROM_HERE, kBatchUploadAvatarButtonOverrideTextDuration,
        base::BindOnce(&BatchUploadService::OnAvatarOverrideTextTimeout,
                       // Unretained is fine here since the timer is a field
                       // member and will not fire if destroyed.
                       base::Unretained(this)));
  }

  // Reset the state of the service.
  controller_.reset();
  browser_ = nullptr;
}

bool BatchUploadService::ShouldShowBatchUploadEntryPointForDataType(
    BatchUploadDataType type) {
  if (!IsUserEligibleToOpenDialog()) {
    return false;
  }

  std::unique_ptr<BatchUploadDataProvider> local_data_provider =
      GetBatchUploadDataProvider(profile_.get(), type);
  return local_data_provider->HasLocalData();
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

void BatchUploadService::OnAvatarOverrideTextTimeout() {
  CHECK(avatar_override_clear_callback_);
  avatar_override_clear_callback_.RunAndReset();
}

bool BatchUploadService::IsDialogOpened() const {
  return controller_ != nullptr;
}
