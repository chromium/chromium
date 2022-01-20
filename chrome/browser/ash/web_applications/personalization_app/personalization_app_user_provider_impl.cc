// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_user_provider_impl.h"

#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace {

GURL GetUserImageDataUrl(const user_manager::User& user) {
  if (user.GetImage().isNull())
    return GURL();
  return GURL(webui::GetBitmapDataUrl(*user.GetImage().bitmap()));
}

}  // namespace

PersonalizationAppUserProviderImpl::PersonalizationAppUserProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {}

PersonalizationAppUserProviderImpl::~PersonalizationAppUserProviderImpl() =
    default;

void PersonalizationAppUserProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::UserProvider>
        receiver) {
  user_receiver_.reset();
  user_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppUserProviderImpl::SetUserImageObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
        observer) {
  // May already be bound if user refreshes page.
  user_image_observer_remote_.reset();
  user_image_observer_remote_.Bind(std::move(observer));
  DCHECK(user_manager::UserManager::IsInitialized());
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager_observer_.IsObserving())
    user_manager_observer_.Observe(user_manager);

  // Call it manually the first time.
  OnUserImageChanged(*ash::ProfileHelper::Get()->GetUserByProfile(profile_));
}

void PersonalizationAppUserProviderImpl::GetUserInfo(
    GetUserInfoCallback callback) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  std::move(callback).Run(ash::personalization_app::UserDisplayInfo(*user));
}

void PersonalizationAppUserProviderImpl::GetDefaultUserImages(
    GetDefaultUserImagesCallback callback) {
  std::vector<ash::default_user_image::DefaultUserImage> images =
      ash::default_user_image::GetCurrentImageSet();
  std::move(callback).Run(std::move(images));
}

void PersonalizationAppUserProviderImpl::SelectDefaultImage(int index) {
  if (!ash::default_user_image::IsInCurrentImageSet(index)) {
    mojo::ReportBadMessage("Invalid user image selected");
    return;
  }

  auto* user_image_manager = ash::ChromeUserManager::Get()->GetUserImageManager(
      chromeos::ProfileHelper::Get()
          ->GetUserByProfile(profile_)
          ->GetAccountId());

  user_image_manager->SaveUserDefaultImageIndex(index);
}

void PersonalizationAppUserProviderImpl::OnUserImageChanged(
    const user_manager::User& user) {
  const user_manager::User* desired_user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(desired_user);

  if (user.GetAccountId() != desired_user->GetAccountId())
    return;

  int image_index = user.image_index();
  // Image is a valid default image and has an internal chrome://theme url.
  if (ash::default_user_image::IsInCurrentImageSet(image_index)) {
    user_image_observer_remote_->OnUserImageChanged(
        ash::default_user_image::GetDefaultImageUrl(image_index));
    return;
  }
  // All other cases.
  user_image_observer_remote_->OnUserImageChanged(GetUserImageDataUrl(user));
}
