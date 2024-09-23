// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/media_app/mahi_media_app_handler_factory.h"

#include <memory>

#include "chrome/browser/ash/mahi/media_app/mahi_media_app_client.h"

namespace ash {

// static
MahiMediaAppHandlerFactory* MahiMediaAppHandlerFactory::GetInstance() {
  static base::NoDestructor<MahiMediaAppHandlerFactory> instance;
  return instance.get();
}

MahiMediaAppHandlerFactory::MahiMediaAppHandlerFactory() = default;
MahiMediaAppHandlerFactory::~MahiMediaAppHandlerFactory() = default;

void MahiMediaAppHandlerFactory::CreateMahiMediaAppUntrustedHandler(
    mojo::PendingReceiver<ash::media_app_ui::mojom::MahiUntrustedPageHandler>
        receiver,
    mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
    const std::string& file_name,
    aura::Window* window) {
  auto mahi_pdf_handler =
      std::make_unique<MahiMediaAppClient>(std::move(page), file_name, window);
  media_app_receivers_.Add(std::move(mahi_pdf_handler), std::move(receiver));
}

}  // namespace ash
