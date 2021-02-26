// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_UI_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_UI_H_

#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

namespace content {
class WebUI;
}  // namespace content

class TerminalUIConfig : public ui::WebUIConfig {
 public:
  TerminalUIConfig();
  ~TerminalUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

class TerminalUI : public ui::UntrustedWebUIController {
 public:
  explicit TerminalUI(content::WebUI* web_ui);
  TerminalUI(const TerminalUI&) = delete;
  TerminalUI& operator=(const TerminalUI&) = delete;
  ~TerminalUI() override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_UI_H_
