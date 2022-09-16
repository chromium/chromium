// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::personalization_app {

class PersonalizationAppThemeProvider : public mojom::ThemeProvider {
 public:
  virtual void BindInterface(
      mojo::PendingReceiver<mojom::ThemeProvider> receiver) = 0;
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_PROVIDER_H_
