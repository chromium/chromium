// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_ROUTER_TAB_HELPER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_ROUTER_TAB_HELPER_H_

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace favicon {
class FaviconDriver;
}

namespace sync_sessions {

class SyncSessionsWebContentsRouter;

// TabHelper class that forwards tab-level WebContentsObserver events to a
// (per-profile) sessions router. The router is responsible for forwarding
// these events to sessions sync.
// A TabHelper is a WebContentsObserver tied to the top level WebContents for a
// browser tab.
// https://chromium.googlesource.com/chromium/src/+/master/docs/tab_helpers.md
class SyncSessionsRouterTabHelper
    : public content::WebContentsUserData<SyncSessionsRouterTabHelper>,
      public content::WebContentsObserver,
      public translate::ContentTranslateDriver::Observer,
      public favicon::FaviconDriverObserver {
 public:
  ~SyncSessionsRouterTabHelper() override;

  static void CreateForWebContents(
      content::WebContents* web_contents,
      SyncSessionsWebContentsRouter* session_router);

  // WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void WebContentsDestroyed() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // ContentTranslateDriver::Observer implementation.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  // favicon::FaviconDriverObserver implementation.
  void OnFaviconUpdated(
      favicon::FaviconDriver* favicon_driver,
      FaviconDriverObserver::NotificationIconType notification_icon_type,
      const GURL& icon_url,
      bool icon_url_changed,
      const gfx::Image& image) override;

 private:
  friend class content::WebContentsUserData<SyncSessionsRouterTabHelper>;

  explicit SyncSessionsRouterTabHelper(content::WebContents* web_contents,
                                       SyncSessionsWebContentsRouter* router);

  void NotifyRouter(bool page_load_completed = false);

  // |router_| is a KeyedService and is guaranteed to outlive |this|.
  SyncSessionsWebContentsRouter* router_;

  ChromeTranslateClient* chrome_translate_client_;

  favicon::FaviconDriver* favicon_driver_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SyncSessionsRouterTabHelper);
};

}  // namespace sync_sessions

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_ROUTER_TAB_HELPER_H_
