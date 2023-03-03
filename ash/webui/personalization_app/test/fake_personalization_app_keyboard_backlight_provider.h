// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_H_

#include "ash/webui/personalization_app/personalization_app_keyboard_backlight_provider.h"

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class FakePersonalizationAppKeyboardBacklightProvider
    : public PersonalizationAppKeyboardBacklightProvider {
 public:
  explicit FakePersonalizationAppKeyboardBacklightProvider(
      content::WebUI* web_ui);

  FakePersonalizationAppKeyboardBacklightProvider(
      const FakePersonalizationAppKeyboardBacklightProvider&) = delete;
  FakePersonalizationAppKeyboardBacklightProvider& operator=(
      const FakePersonalizationAppKeyboardBacklightProvider&) = delete;

  ~FakePersonalizationAppKeyboardBacklightProvider() override;

  // PersonalizationAppAmbientProvider:
  void BindInterface(mojo::PendingReceiver<
                     personalization_app::mojom::KeyboardBacklightProvider>
                         receiver) override;

  void SetKeyboardBacklightObserver(
      mojo::PendingRemote<
          ash::personalization_app::mojom::KeyboardBacklightObserver> observer)
      override {}

  void SetBacklightColor(mojom::BacklightColor backlight_color) override {}

  void SetBacklightZoneColor(int zone,
                             mojom::BacklightColor backlight_color) override {}

  void ShouldShowNudge(ShouldShowNudgeCallback callback) override {}

  void HandleNudgeShown() override {}

 private:
  mojo::Receiver<ash::personalization_app::mojom::KeyboardBacklightProvider>
      ambient_receiver_{this};
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_H_
