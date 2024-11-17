// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_service_factory.h"

#include <memory>

namespace ash {

// static
AXMediaAppServiceFactory* AXMediaAppServiceFactory::GetInstance() {
  static base::NoDestructor<AXMediaAppServiceFactory> instance;
  return instance.get();
}

AXMediaAppServiceFactory::AXMediaAppServiceFactory() = default;
AXMediaAppServiceFactory::~AXMediaAppServiceFactory() = default;

void AXMediaAppServiceFactory::CreateAXMediaAppUntrustedService(
    content::BrowserContext& context,
    gfx::NativeWindow native_window,
    mojo::PendingReceiver<ash::media_app_ui::mojom::OcrUntrustedService>
        receiver,
    mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> page) {
  auto ax_media_app_service = std::make_unique<AXMediaAppUntrustedService>(
      context, native_window, std::move(page));
  media_app_receivers_.Add(std::move(ax_media_app_service),
                           std::move(receiver));
}

}  // namespace ash
