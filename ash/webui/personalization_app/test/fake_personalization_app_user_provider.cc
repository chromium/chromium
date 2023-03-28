// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/fake_personalization_app_user_provider.h"

#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace ash::personalization_app {

FakePersonalizationAppUserProvider::FakePersonalizationAppUserProvider(
    content::WebUI* web_ui) {}

FakePersonalizationAppUserProvider::~FakePersonalizationAppUserProvider() =
    default;

void FakePersonalizationAppUserProvider::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::UserProvider> receiver) {
  user_receiver_.reset();
  user_receiver_.Bind(std::move(receiver));
}

void FakePersonalizationAppUserProvider::SetUserImageObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
        observer) {}

void FakePersonalizationAppUserProvider::GetUserInfo(
    GetUserInfoCallback callback) {
  // auto user_info_ptr = ash::personalization_app::mojom::UserInfo::New();
  // user_info_ptr->email = "fake-email";
  // user_info_ptr->name = "Fake Name";
  ash::personalization_app::UserDisplayInfo display_info;
  display_info.email = "fake-email";
  display_info.name = "Fake Name";
  std::move(callback).Run(std::move(display_info));
}

void FakePersonalizationAppUserProvider::GetDefaultUserImages(
    GetDefaultUserImagesCallback callback) {}

bool FakePersonalizationAppUserProvider::IsCustomizationSelectorsPrefEnabled() {
  return true;
}

void FakePersonalizationAppUserProvider::SelectDefaultImage(int index) {}

void FakePersonalizationAppUserProvider::SelectProfileImage() {}
void FakePersonalizationAppUserProvider::SelectCameraImage(
    ::mojo_base::BigBuffer data) {}

void FakePersonalizationAppUserProvider::SelectImageFromDisk() {}

}  // namespace ash::personalization_app
