// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_UNTRUSTED_UI_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_UNTRUSTED_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"

namespace glic {

class SelectionOverlayController;
class SelectionOverlayUntrustedUI;

class SelectionOverlayUntrustedUIConfig
    : public DefaultTopChromeWebUIConfig<SelectionOverlayUntrustedUI> {
 public:
  SelectionOverlayUntrustedUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                    chrome::kChromeUIGlicUntrustedHost) {}

  // `DefaultTopChromeWebUIConfig`:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for the chrome-untrusted://glic/selection-overlay page.
class SelectionOverlayUntrustedUI
    : public UntrustedTopChromeWebUIController,
      public selection::SelectionOverlayPageHandlerFactory {
 public:
  explicit SelectionOverlayUntrustedUI(content::WebUI* web_ui);

  SelectionOverlayUntrustedUI(const SelectionOverlayUntrustedUI&) = delete;
  SelectionOverlayUntrustedUI& operator=(const SelectionOverlayUntrustedUI&) =
      delete;
  ~SelectionOverlayUntrustedUI() override;

  // Instantiates the implementor of the PageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<selection::SelectionOverlayPageHandlerFactory>
          receiver);

  static constexpr std::string_view GetWebUIName() {
    return "GlicSelectionOverlayUntrusted";
  }

 private:
  SelectionOverlayController& GetSelectionOverlayController();

  // `selection::SelectionOverlayPageHandlerFactory`:
  void CreatePageHandler(
      mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
      mojo::PendingRemote<selection::SelectionOverlayPage> page) override;

  mojo::Receiver<selection::SelectionOverlayPageHandlerFactory>
      selection_page_factory_receiver_{this};

  base::WeakPtrFactory<SelectionOverlayUntrustedUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_UNTRUSTED_UI_H_
