// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

inline constexpr char kContextualTasksUiHost[] = "contextual-tasks";

class ContextualTasksUI : public TopChromeWebUIController,
                          public contextual_tasks::mojom::PageHandlerFactory {
 public:
  explicit ContextualTasksUI(content::WebUI* web_ui);
  ContextualTasksUI(const ContextualTasksUI&) = delete;
  ContextualTasksUI& operator=(const ContextualTasksUI&) = delete;
  ~ContextualTasksUI() override;

  // next::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<contextual_tasks::mojom::Page> page,
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler)
      override;

  void MaybeShowUi();

  void BindInterface(
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandlerFactory>
          pending_receiver);

  static constexpr std::string_view GetWebUIName() { return "ContextualTasks"; }

 private:
  mojo::Receiver<contextual_tasks::mojom::PageHandlerFactory> factory_receiver_{
      this};

  std::unique_ptr<contextual_tasks::mojom::PageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class ContextualTasksUIConfig
    : public DefaultTopChromeWebUIConfig<ContextualTasksUI> {
 public:
  ContextualTasksUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    kContextualTasksUiHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
