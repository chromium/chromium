// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_EVENT_ROUTER_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace extensions {

// Tracks new tab navigations and routes them as events to the extension system.
class WebNavigationEventRouter {
 public:
  explicit WebNavigationEventRouter(Profile* profile);

  WebNavigationEventRouter(const WebNavigationEventRouter&) = delete;
  WebNavigationEventRouter& operator=(const WebNavigationEventRouter&) = delete;

  ~WebNavigationEventRouter();

  // Router level handler for the creation of WebContents. Stores information
  // about the newly created WebContents. This information is later used when
  // the WebContents for the tab is added to the tabstrip and we receive the
  // TabStripModelChanged insertion.
  void RecordNewWebContents(content::WebContents* source_web_contents,
                            int source_render_process_id,
                            int source_render_frame_id,
                            GURL target_url,
                            content::WebContents* target_web_contents,
                            bool not_yet_in_tabstrip);

 private:
  // Used to cache the information about newly created WebContents objects.
  // Will run |on_destroy_| if/when the target WebContents is destroyed.
  class PendingWebContents : public content::WebContentsObserver {
   public:
    PendingWebContents();

    PendingWebContents(const PendingWebContents&) = delete;
    PendingWebContents& operator=(const PendingWebContents&) = delete;

    ~PendingWebContents() override;

    void Set(int source_tab_id,
             int source_render_process_id,
             int source_extension_frame_id,
             content::WebContents* target_web_contents,
             const GURL& target_url,
             base::OnceCallback<void(content::WebContents*)> on_destroy);

    // content::WebContentsObserver:
    void WebContentsDestroyed() override;

    int source_tab_id() const { return source_tab_id_; }
    int source_render_process_id() const { return source_render_process_id_; }
    int source_extension_frame_id() const { return source_extension_frame_id_; }
    content::WebContents* target_web_contents() const {
      return target_web_contents_;
    }
    GURL target_url() const { return target_url_; }

   private:
    // The Extensions API ID for the source tab.
    int source_tab_id_ = -1;
    // The source frame's RenderProcessHost ID.
    int source_render_process_id_ = -1;
    // The Extensions API ID for the source frame.
    int source_extension_frame_id_ = -1;
    raw_ptr<content::WebContents> target_web_contents_ = nullptr;
    GURL target_url_;
    base::OnceCallback<void(content::WebContents*)> on_destroy_;
  };

  // The method takes the details of such an event and creates a JSON formatted
  // extension event from it.
  void TabAdded(content::WebContents* tab);

  // Dispatches the tab replaced extension event.
  void TabReplaced(content::WebContents* old_contents,
                   content::WebContents* new_contents);

  // Removes |tab| from |pending_web_contents_| if it is there.
  void PendingWebContentsDestroyed(content::WebContents* tab);

  // Mapping pointers to WebContents objects to information about how they got
  // created.
  std::map<content::WebContents*, PendingWebContents> pending_web_contents_;

  // The profile that owns us via ExtensionService.
  raw_ptr<Profile> profile_;

  // Handles tab strip differences between Win/Mac/Linux and Android.
  class TabHelper;
  std::unique_ptr<TabHelper> tab_helper_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_EVENT_ROUTER_H_
