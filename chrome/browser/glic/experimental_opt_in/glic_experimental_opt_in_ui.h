// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_

#include <memory>

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace glic {

inline constexpr int kGlicExperimentalOptInDefaultHeightGlic = 452;
inline constexpr int kGlicExperimentalOptInDefaultHeightExperimental = 551;
inline constexpr int kGlicExperimentalOptInDefaultWidth = 512;

class GlicExperimentalOptInPageHandler;
class GlicExperimentalOptInUI;

class GlicExperimentalOptInUIConfig
    : public content::DefaultWebUIConfig<GlicExperimentalOptInUI> {
 public:
  GlicExperimentalOptInUIConfig();
  ~GlicExperimentalOptInUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class GlicExperimentalOptInUI : public ui::MojoWebUIController {
 public:
  explicit GlicExperimentalOptInUI(content::WebUI* web_ui);
  ~GlicExperimentalOptInUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::ExperimentalOptInPageHandler> receiver);

 private:
  std::unique_ptr<GlicExperimentalOptInPageHandler> page_handler_;
  RequiredExperimentalOptIn required_state_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_
