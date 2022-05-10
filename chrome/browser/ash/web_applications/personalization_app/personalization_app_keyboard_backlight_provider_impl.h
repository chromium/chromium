// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_IMPL_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_keyboard_backlight_provider.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class PersonalizationAppKeyboardBacklightProviderImpl
    : public PersonalizationAppKeyboardBacklightProvider {
 public:
  explicit PersonalizationAppKeyboardBacklightProviderImpl(
      content::WebUI* web_ui);

  PersonalizationAppKeyboardBacklightProviderImpl(
      const PersonalizationAppKeyboardBacklightProviderImpl&) = delete;
  PersonalizationAppKeyboardBacklightProviderImpl& operator=(
      const PersonalizationAppKeyboardBacklightProviderImpl&) = delete;

  ~PersonalizationAppKeyboardBacklightProviderImpl() override;

  // PersonalizationAppKeyboardBacklightProvider:
  void BindInterface(mojo::PendingReceiver<
                     ash::personalization_app::mojom::KeyboardBacklightProvider>
                         receiver) override;

  // mojom::PersonalizationAppKeyboardBacklightProvider:
  void SetKeyboardBacklightObserver(
      mojo::PendingRemote<
          ash::personalization_app::mojom::KeyboardBacklightObserver> observer)
      override;

  void SetBacklightColor(
      ash::personalization_app::mojom::BacklightColor backlight_color) override;

 private:
  // Notify webUI the current state of backlight color.
  void NotifyBacklightColorChanged();

  // Pointer to profile of user that opened personalization SWA. Not owned.
  raw_ptr<Profile> const profile_ = nullptr;

  mojo::Receiver<ash::personalization_app::mojom::KeyboardBacklightProvider>
      keyboard_backlight_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::KeyboardBacklightObserver>
      keyboard_backlight_observer_remote_;
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_IMPL_H_
