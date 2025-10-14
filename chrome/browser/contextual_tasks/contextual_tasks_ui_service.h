// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class GURL;

namespace contextual_tasks {

// A service used to coordinate all of the side panel instances showing an AI
// thread. Events like tab switching and Intercepted navigations from both the
// sidepanel and omnibox will be routed here.
class ContextualTasksUiService : public KeyedService {
 public:
  ContextualTasksUiService();
  ContextualTasksUiService(const ContextualTasksUiService&) = delete;
  ContextualTasksUiService operator=(const ContextualTasksUiService&) = delete;
  ~ContextualTasksUiService() override;

  // Gets the host URL for the AI page that is shown in the side panel.
  virtual const GURL& GetAiPageHost();

  // A notification to this service that a link in the AI thread was clicked by
  // the user. This will open a tab and associate it with the visible thread.
  virtual void OnThreadLinkClicked();

 private:
  // The host of the AI page that is loaded into the WebUI.
  GURL ai_page_host_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
