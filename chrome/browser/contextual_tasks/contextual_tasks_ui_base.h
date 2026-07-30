// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_BASE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_BASE_H_

#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_toolbar.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

class Profile;

namespace contextual_tasks {

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
// 2. Managing Mojo IPC bindings (PageHandlerFactory and PageHandler) to allow
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
      public contextual_tasks_toolbar::mojom::PageHandler {
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

  Profile* GetProfile();
  contextual_tasks_toolbar::mojom::Page* GetToolbarPageRemote() {
    return toolbar_page_.get();
  }

 private:
  mojo::Receiver<contextual_tasks_toolbar::mojom::PageHandlerFactory>
      toolbar_page_factory_receiver_{this};
  mojo::Receiver<contextual_tasks_toolbar::mojom::PageHandler>
      toolbar_page_handler_receiver_{this};
  mojo::Remote<contextual_tasks_toolbar::mojom::Page> toolbar_page_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_BASE_H_
