// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

class GoogleServiceAuthError;

namespace base {
class Uuid;
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
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> receiver,
      mojo::PendingRemote<contextual_tasks::mojom::Page> page,
      ContextualTasksUI* web_ui_controller,
      contextual_tasks::ContextualTasksUiService* ui_service);
  ~ContextualTasksPageHandler() override;

  // contextual_tasks::mojom::PageHandler:
  void GetThreadUrl(GetThreadUrlCallback callback) override;
  void GetUrlForTask(const base::Uuid& uuid,
                     GetUrlForTaskCallback callback) override;
  void SetTaskId(const base::Uuid& uuid) override;
  void SetThreadTitle(const std::string& title) override;

  void CloseSidePanel() override;
  void ShowThreadHistory(ShowThreadHistoryCallback callback) override;
  void IsShownInTab(IsShownInTabCallback callback) override;
  void OpenMyActivityUi() override;
  void OpenHelpUi() override;
  void MoveTaskUiToToNewTab() override;
  void GetOAuthToken(GetOAuthTokenCallback callback) override;
  void GetAttachedTabs(GetAttachedTabsCallback callback) override;
  void OnTabClickedFromSourcesMenu(int32_t tab_id, const GURL& url) override;
  void OnWebviewMessage(const std::vector<uint8_t>& message) override;
  void GetHandshakeMessage(GetHandshakeMessageCallback callback) override;
  void PostMessageToWebview(const lens::ClientToAimMessage& message);

 private:
  void OnOAuthTokenReceived(GetOAuthTokenCallback callback,
                            GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  std::unique_ptr<signin::AccessTokenFetcher> oauth_token_fetcher_;
  mojo::Receiver<contextual_tasks::mojom::PageHandler> receiver_;
  mojo::Remote<contextual_tasks::mojom::Page> page_;
  raw_ptr<ContextualTasksUI> web_ui_controller_;
  raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
