// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::personalization_app {

class PersonalizationAppKeyboardBacklightProvider
    : public mojom::KeyboardBacklightProvider {
 public:
  virtual void BindInterface(
      mojo::PendingReceiver<mojom::KeyboardBacklightProvider> receiver) = 0;
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_H_
