// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sync/extension_local_data_batch_uploader.h"

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"
#include "extensions/browser/icon_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr int kBatchUploadIconSize = extension_misc::EXTENSION_ICON_SMALLISH;

// Called when the icon for `extension_id` is loaded. Must be called for all
// uploadable extensions before the `LocalDataDescription` is created.
syncer::LocalDataItemModel OnExtensionIconLoaded(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const gfx::Image& icon) {
  syncer::LocalDataItemModel extension_model;
  extension_model.id = extension_id;
  extension_model.title = extension_name;
  GURL extension_icon_url =
      icon.IsEmpty()
          ? GetPlaceholderIconUrl(kBatchUploadIconSize, extension_name)
          : GetIconUrlFromImage(icon);
  extension_model.icon =
      syncer::LocalDataItemModel::PageUrlIcon(std::move(extension_icon_url));
  return extension_model;
}

// Called when all uploadable extensions' icons have been loaded and a
// `LocalDataItemModel` has been created for each extension.
void OnAllExtensionModelsReady(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback,
    std::vector<syncer::LocalDataItemModel> extension_models) {
  syncer::LocalDataDescription local_data;
  local_data.type = syncer::DataType::EXTENSIONS;
  local_data.local_data_models = std::move(extension_models);

  std::move(callback).Run(std::move(local_data));
}

}  // namespace

ExtensionLocalDataBatchUploader::ExtensionLocalDataBatchUploader(
    Profile* profile)
    : profile_(profile) {}

ExtensionLocalDataBatchUploader::~ExtensionLocalDataBatchUploader() = default;

void ExtensionLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  if (!sync_util::IsSyncingExtensionsInTransportMode(profile_) ||
      !base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    std::move(callback).Run(syncer::LocalDataDescription());
    return;
  }

  std::vector<const Extension*> uploadable_extensions =
      AccountExtensionTracker::Get(profile_)->GetUploadableLocalExtensions();
  if (uploadable_extensions.empty()) {
    std::move(callback).Run(syncer::LocalDataDescription());
    return;
  }

  auto barrier_callback = base::BarrierCallback<syncer::LocalDataItemModel>(
      uploadable_extensions.size(),
      base::BindOnce(&OnAllExtensionModelsReady, std::move(callback)));

  auto* image_loader = ImageLoader::Get(profile_);

  // Load each uploadable extension's icon.
  for (const Extension* extension : uploadable_extensions) {
    ExtensionResource icon = IconsInfo::GetIconResource(
        extension, kBatchUploadIconSize, ExtensionIconSet::Match::kBigger);
    if (icon.empty()) {
      barrier_callback.Run(OnExtensionIconLoaded(
          extension->id(), extension->name(), gfx::Image()));
    } else {
      gfx::Size max_size(kBatchUploadIconSize, kBatchUploadIconSize);
      image_loader->LoadImageAsync(
          extension, icon, max_size,
          base::BindOnce(&OnExtensionIconLoaded, extension->id(),
                         extension->name())
              .Then(barrier_callback));
    }
  }
}

void ExtensionLocalDataBatchUploader::TriggerLocalDataMigration() {
  TriggerLocalDataMigrationForItemsInternal(/*ids_to_upload=*/std::nullopt);
}

void ExtensionLocalDataBatchUploader::TriggerLocalDataMigrationForItems(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  ExtensionIdSet ids_to_upload;
  for (const auto& item_id : items) {
    const std::string* item_id_str = std::get_if<std::string>(&item_id);
    DCHECK(item_id_str);
    ids_to_upload.insert(*item_id_str);
  }

  TriggerLocalDataMigrationForItemsInternal(std::move(ids_to_upload));
}

void ExtensionLocalDataBatchUploader::TriggerLocalDataMigrationForItemsInternal(
    std::optional<ExtensionIdSet> ids_to_upload) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  CHECK(!account_info.IsEmpty());

  if (!sync_util::IsSyncingExtensionsInTransportMode(profile_) ||
      !base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return;
  }

  std::vector<const Extension*> uploadable_extensions =
      AccountExtensionTracker::Get(profile_)->GetUploadableLocalExtensions();

  // If the extension is specified in `ids_to_upload` or if `ids_to_upload` is
  // null (implying no filter), upload it to the user's account.
  for (const Extension* extension : uploadable_extensions) {
    if (!ids_to_upload || ids_to_upload->contains(extension->id())) {
      sync_util::UploadExtensionToAccount(profile_, *extension);
    }
  }
}

}  // namespace extensions
