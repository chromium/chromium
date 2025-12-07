// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_service.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace ash {

// Factory to create an instance of `AXMediaAppUntrustedService` used by the
// Media App (AKA Gallery) to communicate with the accessibility layer.
class AXMediaAppServiceFactory final {
 public:
  static AXMediaAppServiceFactory* GetInstance();

  AXMediaAppServiceFactory(const AXMediaAppServiceFactory&) = delete;
  AXMediaAppServiceFactory& operator=(const AXMediaAppServiceFactory&) = delete;
  ~AXMediaAppServiceFactory();

  void CreateAXMediaAppUntrustedService(
      content::BrowserContext& context,
      gfx::NativeWindow native_window,
      mojo::PendingReceiver<ash::media_app_ui::mojom::OcrUntrustedService>
          receiver,
      mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> page);

  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::OcrUntrustedService>&
  media_app_receivers() {
    return media_app_receivers_;
  }

 private:
  friend base::NoDestructor<AXMediaAppServiceFactory>;

  AXMediaAppServiceFactory();

  // Owns all the receivers for all MediaApp windows each
  // AXMediaAppUntrustedService instance is connected to. If a MediaApp window
  // is destroyed or disconnected, the corresponding entry in this set is also
  // deleted.
  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::OcrUntrustedService>
      media_app_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_SERVICE_FACTORY_H_
