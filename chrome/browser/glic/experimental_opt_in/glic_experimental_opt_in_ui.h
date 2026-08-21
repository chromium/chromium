// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_

#include <memory>

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "content/public/browser/webui_config.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/webui/mojo_web_ui_controller.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "components/guest_view/browser/slim_web_view/slim_web_view_page_handler_factory.h"  // nogncheck
#endif

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

class GlicExperimentalOptInUI :
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    public guest_view::SlimWebViewPageHandlerFactory,
#endif
    public ui::MojoWebUIController {
 public:
  explicit GlicExperimentalOptInUI(content::WebUI* web_ui);
  ~GlicExperimentalOptInUI() override;

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  using SlimWebViewPageHandlerFactory::BindInterface;
#endif

  void BindInterface(
      mojo::PendingReceiver<mojom::ExperimentalOptInPageHandler> receiver);

 private:
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  using SlimWebViewPageHandlerFactory::CreatePageHandler;
#endif

  std::unique_ptr<GlicExperimentalOptInPageHandler> page_handler_;
  RequiredExperimentalOptIn required_state_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_
