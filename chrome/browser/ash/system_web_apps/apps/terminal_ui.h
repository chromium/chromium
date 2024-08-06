// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_TERMINAL_UI_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_TERMINAL_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

class TerminalUI;

class TerminalUIConfig : public content::DefaultWebUIConfig<TerminalUI> {
 public:
  TerminalUIConfig();
  ~TerminalUIConfig() override;
};

class TerminalUI : public ui::UntrustedWebUIController {
 public:
  explicit TerminalUI(content::WebUI* web_ui);
  TerminalUI(const TerminalUI&) = delete;
  TerminalUI& operator=(const TerminalUI&) = delete;
  ~TerminalUI() override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_TERMINAL_UI_H_
