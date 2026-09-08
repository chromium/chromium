// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace browser_actuator {

inline constexpr char kChromeUIBrowserActuatorInternalsHost[] =
    "browser-actuator-internals";

class BrowserActuatorInternalsUIMojoImpl;

// WebUIController for chrome://browser-actuator-internals/.
class BrowserActuatorInternalsUI : public ui::MojoWebUIController,
                                   public browser_actuator_internals::mojom::
                                       BrowserActuatorInternalsUIFactory {
 public:
  explicit BrowserActuatorInternalsUI(content::WebUI* web_ui);
  BrowserActuatorInternalsUI(const BrowserActuatorInternalsUI&) = delete;
  BrowserActuatorInternalsUI& operator=(const BrowserActuatorInternalsUI&) =
      delete;
  ~BrowserActuatorInternalsUI() override;

  // Instantiates the implementor of the
  // browser_actuator_internals::mojom::BrowserActuatorInternalsUIFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          browser_actuator_internals::mojom::BrowserActuatorInternalsUIFactory>
          receiver);

 private:
  // browser_actuator_internals::mojom::BrowserActuatorInternalsUIFactory:
  void CreateUI(
      mojo::PendingRemote<
          browser_actuator_internals::mojom::BrowserActuatorInternalsPage> page,
      mojo::PendingReceiver<
          browser_actuator_internals::mojom::BrowserActuatorInternalsUI> ui)
      override;

  mojo::Receiver<
      browser_actuator_internals::mojom::BrowserActuatorInternalsUIFactory>
      factory_receiver_{this};
  std::unique_ptr<BrowserActuatorInternalsUIMojoImpl> mojo_impl_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class BrowserActuatorInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<BrowserActuatorInternalsUI> {
 public:
  BrowserActuatorInternalsUIConfig();
  ~BrowserActuatorInternalsUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_H_
