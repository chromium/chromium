// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace ash {

// Factory to create an instance of `AXMediaAppUntrustedHandler` used by the
// Media App (AKA Gallery) to communicate with the accessibility layer.
class AXMediaAppHandlerFactory final {
 public:
  static AXMediaAppHandlerFactory* GetInstance();

  AXMediaAppHandlerFactory(const AXMediaAppHandlerFactory&) = delete;
  AXMediaAppHandlerFactory& operator=(const AXMediaAppHandlerFactory&) = delete;
  ~AXMediaAppHandlerFactory();

  void CreateAXMediaAppUntrustedHandler(
      content::BrowserContext& context,
      gfx::NativeWindow native_window,
      mojo::PendingReceiver<ash::media_app_ui::mojom::OcrUntrustedPageHandler>
          receiver,
      mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> page);

  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::OcrUntrustedPageHandler>&
  media_app_receivers() {
    return media_app_receivers_;
  }

 private:
  friend base::NoDestructor<AXMediaAppHandlerFactory>;

  AXMediaAppHandlerFactory();

  // Owns all the receivers for all MediaApp windows each
  // AXMediaAppUntrustedHandler instance is connected to. If a MediaApp window
  // is destroyed or disconnected, the corresponding entry in this set is also
  // deleted.
  mojo::UniqueReceiverSet<ash::media_app_ui::mojom::OcrUntrustedPageHandler>
      media_app_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_FACTORY_H_
