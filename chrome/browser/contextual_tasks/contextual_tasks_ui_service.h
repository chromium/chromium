// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Uuid;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {

class ContextualTasksContextController;

// A service used to coordinate all of the side panel instances showing an AI
// thread. Events like tab switching and Intercepted navigations from both the
// sidepanel and omnibox will be routed here.
class ContextualTasksUiService : public KeyedService {
 public:
  ContextualTasksUiService(
      Profile* profile,
      ContextualTasksContextController* context_controller);
  ContextualTasksUiService(const ContextualTasksUiService&) = delete;
  ContextualTasksUiService operator=(const ContextualTasksUiService&) = delete;
  ~ContextualTasksUiService() override;

  // A notification that the browser attempted to navigate to the AI page. If
  // this method is being called, it means the navigation was blocked and it
  // should be processed by this method.
  virtual void OnNavigationToAiPageIntercepted(
      const GURL& url,
      base::WeakPtr<tabs::TabInterface> tab,
      bool is_to_new_tab);

  // A notification to this service that a link in the AI thread was clicked by
  // the user. This will open a tab and associate it with the visible thread.
  virtual void OnThreadLinkClicked(const GURL& url,
                                   base::Uuid task_id,
                                   base::WeakPtr<tabs::TabInterface> tab);

  // A notification that a navigation is occurring. This method gives the
  // service the opportunity to prevent the navigation from happening in order
  // to handle it manually. Returns true if the navigation is being handled by
  // the service (e.g. the navigation is blocked), and false otherwise. The
  // WebContents the navigation originated from is provided along with
  // `is_to_new_tab` which indicates whether the navigation would open in a
  // new tab or window.
  virtual bool HandleNavigation(
      const GURL& navigation_url,
      const GURL& responsible_web_contents_url,
      const content::FrameTreeNodeId& source_frame_tree_node_id,
      bool is_to_new_tab);

  // Returns the URL that a task was created for. Once this is retrieved, the
  // entry is removed from the cache.
  virtual GURL GetInitialUrlForTask(const base::Uuid& uuid);

  // Returns the URL for the default AI page. This is the URL that should be
  // loaded in the absence of any other context.
  virtual GURL GetDefaultAiPageUrl();

  // Returns whether the provided URL is to an AI page.
  bool IsAiUrl(const GURL& url);

 private:
  // Associates a WebContents with a task, assuming the URL of the WebContents'
  // main frame or side panel is a contextual task URL.
  void AssociateWebContentsToTask(content::WebContents* web_contents,
                                  const base::Uuid& task_id);

  const raw_ptr<Profile> profile_;

  raw_ptr<contextual_tasks::ContextualTasksContextController>
      context_controller_;

  // The host of the AI page that is loaded into the WebUI.
  GURL ai_page_host_;

  // Map a task's ID to the URL that was used to create it, if it exists. This
  // is primarily used in init flows where the contextual tasks UI is
  // intercepting a query from some other surface like the omnibox. The entry
  // in this map is removed once the UI is loaded with the correct thread.
  std::map<base::Uuid, GURL> task_id_to_creation_url_;

  base::WeakPtrFactory<ContextualTasksUiService> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
