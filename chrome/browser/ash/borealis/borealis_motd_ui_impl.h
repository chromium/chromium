// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_MOTD_UI_IMPL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_MOTD_UI_IMPL_H_

#include "ash/constants/url_constants.h"
#include "ash/constants/webui_url_constants.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_ui.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace borealis {

// Forward declaration so that config definition can come before controller.
class BorealisMotdUiImpl;

class BorealisMOTDUIConfig
    : public content::DefaultWebUIConfig<BorealisMotdUiImpl> {
 public:
  BorealisMOTDUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           ash::kChromeUIBorealisMOTDHost) {}
};

class BorealisMotdUiImpl : public BorealisMOTDUI {
 public:
  explicit BorealisMotdUiImpl(content::WebUI* web_ui);

  // ash::borealis_motd::mojom::PageHandlerFactory implementation.
  void CreatePageHandler(
      mojo::PendingRemote<ash::borealis_motd::mojom::Page> pending_page,
      mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandler>
          pending_page_handler) override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_MOTD_UI_IMPL_H_
