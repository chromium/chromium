// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_HANDLER_FACTORY_H_
#define CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_HANDLER_FACTORY_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "ui/aura/window.h"

namespace ash {

// Factory class to create instances of `MahiMediaAppClient` that will be called
// by Media App (Gallery) for Mahi support.
class MahiMediaAppHandlerFactory final {
 public:
  static MahiMediaAppHandlerFactory* GetInstance();

  MahiMediaAppHandlerFactory(const MahiMediaAppHandlerFactory&) = delete;
  MahiMediaAppHandlerFactory& operator=(const MahiMediaAppHandlerFactory&) =
      delete;
  ~MahiMediaAppHandlerFactory();

  void CreateMahiMediaAppUntrustedHandler(
      mojo::PendingReceiver<ash::media_app_ui::mojom::MahiUntrustedPageHandler>
          receiver,
      mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
      const std::string& file_name,
      aura::Window* window);

  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::MahiUntrustedPageHandler>&
  media_app_receivers() {
    return media_app_receivers_;
  }

 private:
  friend base::NoDestructor<MahiMediaAppHandlerFactory>;

  MahiMediaAppHandlerFactory();

  // Owns all the receivers for all MediaApp windows each
  // MahiMediaAppUntrustedHandler instance is connected to. If a MediaApp window
  // is destroyed or disconnected, the corresponding entry in this set is also
  // deleted.
  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::MahiUntrustedPageHandler>
      media_app_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_HANDLER_FACTORY_H_
