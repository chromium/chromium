// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_handler_factory.h"

#include <memory>

namespace ash {

// static
AXMediaAppHandlerFactory* AXMediaAppHandlerFactory::GetInstance() {
  static base::NoDestructor<AXMediaAppHandlerFactory> instance;
  return instance.get();
}

AXMediaAppHandlerFactory::AXMediaAppHandlerFactory() = default;
AXMediaAppHandlerFactory::~AXMediaAppHandlerFactory() = default;

void AXMediaAppHandlerFactory::CreateAXMediaAppUntrustedHandler(
    content::BrowserContext& context,
    gfx::NativeWindow native_window,
    mojo::PendingReceiver<ash::media_app_ui::mojom::OcrUntrustedPageHandler>
        receiver,
    mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> page) {
  auto ax_media_app_handler = std::make_unique<AXMediaAppUntrustedHandler>(
      context, native_window, std::move(page));
  media_app_receivers_.Add(std::move(ax_media_app_handler),
                           std::move(receiver));
}

}  // namespace ash
