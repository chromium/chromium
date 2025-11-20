// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class GoogleServiceAuthError;

namespace base {
class Uuid;
}

namespace content {
class WebUI;
}

class ContextualTasksUI;

namespace contextual_tasks {
class ContextualTasksUiService;
}  // namespace contextual_tasks

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

class ContextualTasksPageHandler : public contextual_tasks::mojom::PageHandler {
 public:
  ContextualTasksPageHandler(
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler,
      content::WebUI* web_ui,
      ContextualTasksUI* web_ui_controller,
      contextual_tasks::ContextualTasksUiService* contextual_tasks_ui_service);
  ContextualTasksPageHandler(const ContextualTasksPageHandler&) = delete;
  ContextualTasksPageHandler& operator=(const ContextualTasksPageHandler&) =
      delete;
  ~ContextualTasksPageHandler() override;

  // contextual_tasks::mojom::PageHandler impl:
  void GetThreadUrl(GetThreadUrlCallback callback) override;

  void GetUrlForTask(const base::Uuid& uuid,
                     GetUrlForTaskCallback callback) override;
  void SetTaskId(const base::Uuid& uuid) override;
  void SetThreadTitle(const std::string& title) override;

  void CloseSidePanel() override;
  void ShowThreadHistory(ShowThreadHistoryCallback callback) override;
  void IsShownInTab(IsShownInTabCallback callback) override;
  void OpenChromeSettingsUi() override;
  void OpenMyActivityUi() override;
  void OpenHelpUi() override;
  void MoveTaskUiToToNewTab() override;
  void GetOAuthToken(GetOAuthTokenCallback callback) override;

 private:
  void OnOAuthTokenReceived(GetOAuthTokenCallback callback,
                            GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  mojo::Receiver<contextual_tasks::mojom::PageHandler> page_handler_;
  const raw_ref<content::WebUI> web_ui_;
  const raw_ref<ContextualTasksUI> web_ui_controller_;
  const raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;
  std::unique_ptr<signin::AccessTokenFetcher> oauth_token_fetcher_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
