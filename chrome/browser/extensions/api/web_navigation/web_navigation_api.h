// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions WebNavigation API functions for observing and
// intercepting navigation events, as specified in the extension JSON API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

namespace extensions {

// Tab contents observer that forwards navigation events to the event router.
class WebNavigationTabObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebNavigationTabObserver> {
 public:
  ~WebNavigationTabObserver() override;

  // Returns the object for the given |web_contents|.
  static WebNavigationTabObserver* Get(content::WebContents* web_contents);

  const FrameNavigationState& frame_navigation_state() const {
    return navigation_state_;
  }

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void FrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void WebContentsDestroyed() override;

  // This method dispatches the already created onBeforeNavigate event.
  void DispatchCachedOnBeforeNavigate();

 private:
  explicit WebNavigationTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebNavigationTabObserver>;

  void HandleCommit(content::NavigationHandle* navigation_handle);
  void HandleError(content::NavigationHandle* navigation_handle);

  // True if the transition and target url correspond to a reference fragment
  // navigation.
  bool IsReferenceFragmentNavigation(content::RenderFrameHost* frame_host,
                                     const GURL& url);

  // Called when a RenderFrameHost goes into pending deletion. Stop tracking it
  // and its children.
  void RenderFrameHostPendingDeletion(content::RenderFrameHost*);

  // Tracks the state of the frames we are sending events for.
  FrameNavigationState navigation_state_;

  // The latest onBeforeNavigate event this frame has generated. It is stored
  // as it might not be sent immediately, but delayed until the tab is added to
  // the tab strip and is ready to dispatch events.
  std::unique_ptr<Event> pending_on_before_navigate_event_;

  // Used for tracking registrations to redirect notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationTabObserver);
};

// Observes navigation notifications and routes them as events to the extension
// system.
class WebNavigationEventRouter : public TabStripModelObserver,
                                 public BrowserTabStripTrackerDelegate,
                                 public content::NotificationObserver {
 public:
  explicit WebNavigationEventRouter(Profile* profile);
  ~WebNavigationEventRouter() override;

  // Router level handler for the creation of WebContents. Stores information
  // about the newly created WebContents. This information is later used when
  // the WebContents for the tab is added to the tabstrip and we receive the
  // TAB_ADDED notification.
  void RecordNewWebContents(content::WebContents* source_web_contents,
                            int source_render_process_id,
                            int source_render_frame_id,
                            GURL target_url,
                            content::WebContents* target_web_contents,
                            bool not_yet_in_tabstrip);

 private:
  // Used to cache the information about newly created WebContents objects.
  struct PendingWebContents{
    PendingWebContents();
    PendingWebContents(content::WebContents* source_web_contents,
                       content::RenderFrameHost* source_frame_host,
                       content::WebContents* target_web_contents,
                       const GURL& target_url);
    ~PendingWebContents();

    content::WebContents* source_web_contents;
    content::RenderFrameHost* source_frame_host;
    content::WebContents* target_web_contents;
    GURL target_url;
  };

  // BrowserTabStripTrackerDelegate implementation.
  bool ShouldTrackBrowser(Browser* browser) override;

  // TabStripModelObserver implementation.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Handler for the NOTIFICATION_TAB_ADDED event. The method takes the details
  // of such an event and creates a JSON formated extension event from it.
  void TabAdded(content::WebContents* tab);

  // Handler for NOTIFICATION_WEB_CONTENTS_DESTROYED. If |tab| is in
  // |pending_web_contents_|, it is removed.
  void TabDestroyed(content::WebContents* tab);

  // Mapping pointers to WebContents objects to information about how they got
  // created.
  std::map<content::WebContents*, PendingWebContents> pending_web_contents_;

  // Used for tracking registrations to navigation notifications.
  content::NotificationRegistrar registrar_;

  // The profile that owns us via ExtensionService.
  Profile* profile_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationEventRouter);
};

// API function that returns the state of a given frame.
class WebNavigationGetFrameFunction : public UIThreadExtensionFunction {
  ~WebNavigationGetFrameFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getFrame", WEBNAVIGATION_GETFRAME)
};

// API function that returns the states of all frames in a given tab.
class WebNavigationGetAllFramesFunction : public UIThreadExtensionFunction {
  ~WebNavigationGetAllFramesFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getAllFrames",
                             WEBNAVIGATION_GETALLFRAMES)
};

class WebNavigationAPI : public BrowserContextKeyedAPI,
                         public extensions::EventRouter::Observer {
 public:
  explicit WebNavigationAPI(content::BrowserContext* context);
  ~WebNavigationAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<WebNavigationAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<WebNavigationAPI>;
  friend class WebNavigationTabObserver;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "WebNavigationAPI";
  }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<WebNavigationEventRouter> web_navigation_event_router_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
