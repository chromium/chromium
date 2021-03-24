// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_personalization_app_ui_delegate.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

ChromePersonalizationAppUiDelegate::ChromePersonalizationAppUiDelegate(
    content::WebUI* web_ui) {}

ChromePersonalizationAppUiDelegate::~ChromePersonalizationAppUiDelegate() =
    default;

void ChromePersonalizationAppUiDelegate::BindInterface(
    mojo::PendingReceiver<
        chromeos::personalization_app::mojom::WallpaperProvider> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ChromePersonalizationAppUiDelegate::Initialize(
    InitializeCallback callback) {
  std::move(callback).Run(/*success=*/true);
}
