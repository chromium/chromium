// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_USER_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_USER_PROVIDER_H_

#include "ash/webui/personalization_app/personalization_app_user_provider.h"

#include <stdint.h>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class FakePersonalizationAppUserProvider
    : public PersonalizationAppUserProvider {
 public:
  explicit FakePersonalizationAppUserProvider(content::WebUI* web_ui);

  FakePersonalizationAppUserProvider(
      const FakePersonalizationAppUserProvider&) = delete;
  FakePersonalizationAppUserProvider& operator=(
      const FakePersonalizationAppUserProvider&) = delete;

  ~FakePersonalizationAppUserProvider() override;

  // PersonalizationAppUserProvider:
  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::UserProvider> receiver)
      override;

  // personalization_app::mojom::UserProvider
  void SetUserImageObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
          observer) override;
  void GetUserInfo(GetUserInfoCallback callback) override;
  void GetDefaultUserImages(GetDefaultUserImagesCallback callback) override;
  bool IsCustomizationSelectorsPrefEnabled() override;
  void SelectDefaultImage(int index) override;
  void SelectProfileImage() override;
  void SelectCameraImage(::mojo_base::BigBuffer data) override;
  void SelectImageFromDisk() override;
  void SelectLastExternalUserImage() override {}

 private:
  mojo::Receiver<ash::personalization_app::mojom::UserProvider> user_receiver_{
      this};
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_USER_PROVIDER_H_
