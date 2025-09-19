// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

inline constexpr char kContextualTasksUiHost[] = "contextual-tasks";

class ContextualTasksUI : public ui::MojoWebUIController {
 public:
  explicit ContextualTasksUI(content::WebUI* web_ui);
  ContextualTasksUI(const ContextualTasksUI&) = delete;
  ContextualTasksUI& operator=(const ContextualTasksUI&) = delete;
  ~ContextualTasksUI() override;

  static constexpr std::string_view GetWebUIName() {
    return "Contextual Tasks";
  }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

class ContextualTasksUIConfig : public content::WebUIConfig {
 public:
  ContextualTasksUIConfig()
      : WebUIConfig(content::kChromeUIScheme, kContextualTasksUiHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
