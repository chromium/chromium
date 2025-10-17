// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

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

  // A notification that the browser attempted to navigate to the AI page. If
  // this method is being called, it means the navigation was blocked and it
  // should be processed by this method.
  virtual void OnNavigationToAiPageIntercepted(
      const GURL& url,
      content::WebContents* source_contents,
      bool is_to_new_tab);

  // A notification to this service that a link in the AI thread was clicked by
  // the user. This will open a tab and associate it with the visible thread.
  virtual void OnThreadLinkClicked(const GURL& url,
                                   content::WebContents* source_contents);

  // A notification that a navigation is occurring. This method gives the
  // service the opportunity to prevent the navigation from happening in order
  // to handle it manually. Returns true if the navigation is being handled by
  // the service (e.g. the navigation is blocked), and false otherwise. The
  // WebContents the navigation originated from is provided along with
  // `is_to_new_tab` which indicates whether the navigation would open in a
  // new tab or window.
  virtual bool HandleNavigation(const GURL& navigation_url,
                                const GURL& responsible_web_contents_url,
                                content::WebContents* navigating_contents,
                                bool is_to_new_tab);

  // Returns the URL for the default AI page. This is the URL that should be
  // loaded in the absence of any other context.
  virtual GURL GetDefaultAiPageUrl();

 private:
  // Returns whether the provided URL is to an AI page.
  bool IsAiUrl(const GURL& url);

  // The host of the AI page that is loaded into the WebUI.
  GURL ai_page_host_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
