// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class BrowserWindowInterface;

namespace content {
class BrowserContext;
class WebContentsObserver;
}  // namespace content

namespace contextual_tasks {
class ContextualTasksContextController;
class ContextualTasksUiService;
}  // namespace contextual_tasks

class ContextualTasksComposeboxHandler;
class ContextualTasksInternalsPageHandler;

// An interface for managing task IDs held by the WebUI.
class TaskInfoDelegate {
 public:
  TaskInfoDelegate() = default;
  virtual ~TaskInfoDelegate() = default;
  virtual const std::optional<base::Uuid>& GetTaskId() = 0;
  virtual void SetTaskId(std::optional<base::Uuid> id) = 0;
  virtual const std::optional<std::string>& GetThreadId() = 0;
  virtual void SetThreadId(std::optional<std::string> id) = 0;
  virtual const std::optional<std::string>& GetThreadTitle() = 0;
  virtual void SetThreadTitle(std::optional<std::string> title) = 0;
  virtual bool IsShownInTab() = 0;
  virtual BrowserWindowInterface* GetBrowser() = 0;
  virtual content::WebContents* GetWebUIWebContents() = 0;
};

class ContextualTasksUI : public TaskInfoDelegate,
                          public TopChromeWebUIController,
                          public contextual_tasks::mojom::PageHandlerFactory,
                          public composebox::mojom::PageHandlerFactory,
                          public contextual_tasks_internals::mojom::
                              ContextualTasksInternalsPageHandlerFactory {
 public:
  // A WebContentsObserver used to observe navigations or URL changes in the
  // frame being hosted by this WebUI. Top-level navigations are ignored since
  // this class is only intended to listen to the embedded AI frame.
  class FrameNavObserver : public content::WebContentsObserver {
   public:
    explicit FrameNavObserver(
        content::WebContents* web_contents,
        contextual_tasks::ContextualTasksUiService* ui_service,
        contextual_tasks::ContextualTasksContextController* context_controller,
        TaskInfoDelegate* task_info_delegate);
    ~FrameNavObserver() override = default;

    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;

   private:
    raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;
    raw_ptr<contextual_tasks::ContextualTasksContextController>
        context_controller_;
    raw_ref<TaskInfoDelegate> task_info_delegate_;
  };

  // Enum representing the upload status of tab context.
  enum class TabContextStatus {
    kNotUploaded,
    kPendingUpload,
    kUploaded,
    kIgnored,
  };

  explicit ContextualTasksUI(content::WebUI* web_ui);
  ContextualTasksUI(const ContextualTasksUI&) = delete;
  ContextualTasksUI& operator=(const ContextualTasksUI&) = delete;
  ~ContextualTasksUI() override;

  // next::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<contextual_tasks::mojom::Page> page,
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler)
      override;

  // TaskInfoDelegate impl:
  const std::optional<base::Uuid>& GetTaskId() override;
  void SetTaskId(std::optional<base::Uuid> id) override;
  const std::optional<std::string>& GetThreadId() override;
  void SetThreadId(std::optional<std::string> id) override;
  const std::optional<std::string>& GetThreadTitle() override;
  void SetThreadTitle(std::optional<std::string> title) override;
  bool IsShownInTab() override;
  BrowserWindowInterface* GetBrowser() override;
  content::WebContents* GetWebUIWebContents() override;

  void CloseSidePanel();

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

  // Instantiates the implementor of the contextual_tasks::mojom::
  // ContextualTasksInternalsPageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<contextual_tasks_internals::mojom::
                                ContextualTasksInternalsPageHandlerFactory>
          pending_receiver);

  // contextual_tasks::mojom::ContextualTasksInternalsPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<
          contextual_tasks_internals::mojom::ContextualTasksInternalsPage> page,
      mojo::PendingReceiver<contextual_tasks_internals::mojom::
                                ContextualTasksInternalsPageHandler> receiver)
      override;

  static constexpr std::string_view GetWebUIName() { return "ContextualTasks"; }

  // Notify the UI that the WebContents has moved to or from the side panel or
  // tab.
  void OnSidePanelStateChanged();

  // Called when the active tab has been changed, either a new page is loaded or
  // a title change. This is only called when the of this class is rendered in
  // the side panel.
  void OnActiveTabContextStatusChanged(TabContextStatus status);

  void SetComposeboxHandlerForTesting(
      std::unique_ptr<ContextualTasksComposeboxHandler> handler) {
    composebox_handler_ = std::move(handler);
  }

 private:
  // A an observer specifically to watch for the creation of the hosted remote
  // page. This is attached to the WebContents for the WebUI and notifies the
  // WebUI when an inner WebContents is created. The expectation is that there
  // is only ever one inner WebContents at a time.
  class InnerFrameCreationObvserver : public content::WebContentsObserver {
   public:
    explicit InnerFrameCreationObvserver(
        content::WebContents* web_contents,
        base::OnceCallback<void(content::WebContents*)> callback);
    ~InnerFrameCreationObvserver() override;

    void InnerWebContentsCreated(
        content::WebContents* inner_web_contents) override;

   private:
    base::OnceCallback<void(content::WebContents*)> callback_;
  };

  // A notification that the WebContents hosting the WebUI has created an inner
  // WebContents. In practice, this is the creation of the WebContents hosting
  // the embedded remote page.
  void OnInnerWebContentsCreated(content::WebContents* inner_contents);

  std::unique_ptr<ContextualTasksComposeboxHandler> composebox_handler_;
  raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;

  // A handle to the class that extends the ContextualTasksService - the backend
  // component responsible for maintaining associations between open tabs and
  // threads.
  raw_ptr<contextual_tasks::ContextualTasksContextController>
      context_controller_;

  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_handler_factory_receiver_{this};

  mojo::Receiver<contextual_tasks::mojom::PageHandlerFactory>
      contextual_tasks_page_handler_factory_receiver_{this};

  std::unique_ptr<ContextualTasksPageHandler> page_handler_;

  std::unique_ptr<InnerFrameCreationObvserver>
      inner_web_contents_creation_observer_;
  std::unique_ptr<FrameNavObserver> nav_observer_;

  // The ID of the task (concept that owns one or more threads) associated with
  // this WebUI, if it exists. This is a cached value tied to the most recent
  // information we received from observing URL changes on the embedded page.
  // This will be empty for new threads (see below) or when loading a thread
  // that doesn't already have a task. If this value is changing, it is very
  // likely that `thread_id` should also change.
  std::optional<base::Uuid> task_id_;

  // The ID of the thread (concept representing a single session with an AI)
  // associated with this WebUI, if it exists. This will be empty for a new
  // thread and is used to detect changes in the embedded page. If this value is
  // changing, it is very likely that `task_id` should also change.
  std::optional<std::string> thread_id_;

  std::optional<std::string> thread_title_;

  mojo::Remote<contextual_tasks::mojom::Page> page_;

  mojo::Receiver<contextual_tasks_internals::mojom::
                     ContextualTasksInternalsPageHandlerFactory>
      contextual_tasks_internals_page_handler_receiver_{this};

  std::unique_ptr<ContextualTasksInternalsPageHandler>
      contextual_tasks_internals_page_handler_;

  base::WeakPtrFactory<ContextualTasksUI> weak_ptr_factory_{this};

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
