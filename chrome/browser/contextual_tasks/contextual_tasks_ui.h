// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace content {
class BrowserContext;
class WebContentsObserver;
}  // namespace content

namespace contextual_tasks {
class ContextualTasksUiService;
}  // namespace contextual_tasks

class ContextualTasksComposeboxHandler;

class ContextualTasksUI : public TopChromeWebUIController,
                          public contextual_tasks::mojom::PageHandlerFactory,
                          public composebox::mojom::PageHandlerFactory {
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

  void SetTaskId(const base::Uuid& task_id);

  void SetThreadTitle(std::string_view title);

  void MaybeShowUi();

  void BindInterface(
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandlerFactory>
          pending_receiver);

  // composebox::mojom::PageHandlerFactory
  // Instantiates the implementor of the composebox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void CreatePageHandler(
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<composebox::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override;

  // Instantiates the implementor of the
  // composebox::mojom::PageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver);

  static constexpr std::string_view GetWebUIName() { return "ContextualTasks"; }

 private:
  // A WebContentsObserver used to observe navigations or URL changes in the
  // frame being hosted by this WebUI. We're not interested in top-level
  // navigations, so we'll only notify the UI service for navigations that are
  // not in the primary main frame.
  class FrameNavObserver : public content::WebContentsObserver {
   public:
    explicit FrameNavObserver(content::WebContents* web_contents,
                              ContextualTasksUI* ui_handle);
    ~FrameNavObserver() override = default;

    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;

   private:
    raw_ref<ContextualTasksUI> ui_handle_;
  };

  std::unique_ptr<ContextualTasksComposeboxHandler> composebox_handler_;
  raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;

  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_handler_factory_receiver_{this};

  mojo::Receiver<contextual_tasks::mojom::PageHandlerFactory>
      contextual_tasks_page_handler_factory_receiver_{this};

  std::unique_ptr<contextual_tasks::mojom::PageHandler> page_handler_;

  std::unique_ptr<FrameNavObserver> nav_observer_;

  // The ID of the task associated with this WebUI, if it exists.
  std::optional<base::Uuid> task_id_;

  std::optional<std::string> thread_title_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class ContextualTasksUIConfig
    : public DefaultTopChromeWebUIConfig<ContextualTasksUI> {
 public:
  ContextualTasksUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUIContextualTasksHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
