// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_SERVICE_FACTORY_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "ui/aura/window.h"

namespace ash {

// Factory class to create instances of `MahiMediaAppClient` that will be called
// by Media App (Gallery) for Mahi support.
class MahiMediaAppServiceFactory final {
 public:
  static MahiMediaAppServiceFactory* GetInstance();

  MahiMediaAppServiceFactory(const MahiMediaAppServiceFactory&) = delete;
  MahiMediaAppServiceFactory& operator=(const MahiMediaAppServiceFactory&) =
      delete;
  ~MahiMediaAppServiceFactory();

  void CreateMahiMediaAppUntrustedService(
      mojo::PendingReceiver<ash::media_app_ui::mojom::MahiUntrustedService>
          receiver,
      mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
      const std::string& file_name,
      aura::Window* window);

  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::MahiUntrustedService>&
  media_app_receivers() {
    return media_app_receivers_;
  }

 private:
  friend base::NoDestructor<MahiMediaAppServiceFactory>;

  MahiMediaAppServiceFactory();

  // Owns all the receivers for all MediaApp windows each
  // MahiMediaAppUntrustedService instance is connected to. If a MediaApp window
  // is destroyed or disconnected, the corresponding entry in this set is also
  // deleted.
  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::MahiUntrustedService>
      media_app_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_SERVICE_FACTORY_H_
