// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_BASE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_BASE_H_

#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_toolbar.mojom.h"
#include "components/user_education/webui/help_bubble_handler.h"  // nogncheck
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"  // nogncheck

class BrowserWindowInterface;

namespace content {
class WebContents;
class WebUI;
class WebUIDataSource;
}  // namespace content

class Profile;

namespace contextual_tasks {

class ContextualTasksPanelController;
class ContextualTasksPermissionController;

// Base WebUI controller class for Contextual Tasks.
//
// Following the Contextual Tasks rearchitecture, the side panel is split into
// a top toolbar (rendered via WebUI) and a bottom WebView (hosting the page
// content).
//
// This class serves as the base controller for the toolbar WebUI. It is
// responsible for:
// 1. Setting up the WebUIDataSource for `chrome://contextual-tasks` and
//    `chrome://contextual-tasks/internals`.
// 2. Managing Mojo IPC bindings (ContextualTasksToolbarUIService) to allow
//    the WebUI to communicate with the browser process.
//
// Derived classes:
// - `ContextualTasksUIPostRearchitecture`: The new WebUI controller used
//   after the rearchitecture.
// - `ContextualTasksUI`: The legacy WebUI controller, which will be deprecated
//   and removed post-rearchitecture.
class ContextualTasksUIBase
    : public ui::MojoWebUIController,
      public contextual_tasks_toolbar::mojom::PageHandlerFactory,
      public contextual_tasks_toolbar::mojom::PageHandler,
      public contextual_tasks_toolbar::mojom::ContextualTasksToolbarUIService,
      public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit ContextualTasksUIBase(content::WebUI* web_ui);
  ContextualTasksUIBase(const ContextualTasksUIBase&) = delete;
  ContextualTasksUIBase& operator=(const ContextualTasksUIBase&) = delete;
  ~ContextualTasksUIBase() override;

  static content::WebUIDataSource* RegisterWebUIDataSource(Profile* profile);
  static base::DictValue GetContextualTasksLoadTimeData(Profile* profile);

  // contextual_tasks_toolbar::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<contextual_tasks_toolbar::mojom::Page> page,
      mojo::PendingReceiver<contextual_tasks_toolbar::mojom::PageHandler>
          page_handler) override;

  void BindInterface(
      mojo::PendingReceiver<contextual_tasks_toolbar::mojom::PageHandlerFactory>
          pending_receiver);

  // Instantiates and binds the Mojo receiver for
  // ContextualTasksToolbarUIService.
  void BindInterface(
      mojo::PendingReceiver<
          contextual_tasks_toolbar::mojom::ContextualTasksToolbarUIService>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  // contextual_tasks_toolbar::mojom::ContextualTasksToolbarUIService:
  void GetInitialState(GetInitialStateCallback callback) override;
  void OnChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnChipClicked(toolbar_ui_api::mojom::LhsChipIdentifier identifier,
                     bool is_mouse_interaction) override;
  void OnChipPointerEntered(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnChipPointerExited(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;

  Profile* GetProfile();
  content::WebContents* GetWebUIWebContents();
  BrowserWindowInterface* GetBrowser();
  ContextualTasksPanelController* GetPanelController();
  contextual_tasks_toolbar::mojom::Page* GetToolbarPageRemote() {
    return toolbar_page_.get();
  }

  // Returns the Contextual Tasks toolbar WebUI controller hosted by
  // `web_contents`, or null if `web_contents` does not host one.
  static ContextualTasksUIBase* FromWebContents(
      content::WebContents* web_contents);

  // Called by `ContextualTasksPermissionController` when its permission
  // dashboard state changes. Forwards the new state to subscribed WebUI
  // clients, and does nothing if `controller` belongs to a task that is not
  // currently on screen.
  void NotifyPermissionDashboardStateChanged(
      ContextualTasksPermissionController* controller);

 protected:
  // Helper to dynamically resolve the active tab's permission controller.
  virtual ContextualTasksPermissionController* GetActiveController();

 private:
  mojo::Receiver<contextual_tasks_toolbar::mojom::PageHandlerFactory>
      toolbar_page_factory_receiver_{this};
  mojo::Receiver<contextual_tasks_toolbar::mojom::PageHandler>
      toolbar_page_handler_receiver_{this};
  mojo::Remote<contextual_tasks_toolbar::mojom::Page> toolbar_page_;

  mojo::Receiver<
      contextual_tasks_toolbar::mojom::ContextualTasksToolbarUIService>
      receiver_{this};
  mojo::RemoteSet<
      contextual_tasks_toolbar::mojom::ContextualTasksToolbarUIObserver>
      toolbar_ui_observers_;
  toolbar_ui_api::mojom::PermissionDashboardStatePtr
      last_pushed_permission_dashboard_state_;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_factory_receiver_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_BASE_H_
