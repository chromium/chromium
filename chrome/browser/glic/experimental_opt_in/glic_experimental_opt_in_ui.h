// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace glic {

class GlicExperimentalOptInUI;

class GlicExperimentalOptInUIConfig
    : public content::DefaultWebUIConfig<GlicExperimentalOptInUI> {
 public:
  GlicExperimentalOptInUIConfig();
  ~GlicExperimentalOptInUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class GlicExperimentalOptInUI : public content::WebUIController {
 public:
  explicit GlicExperimentalOptInUI(content::WebUI* web_ui);
  ~GlicExperimentalOptInUI() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_H_
