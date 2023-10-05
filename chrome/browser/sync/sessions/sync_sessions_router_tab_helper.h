// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_ROUTER_TAB_HELPER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_ROUTER_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/translate/core/browser/translate_driver.h"
#include "content/public/browser/visibility.h"
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
// https://chromium.googlesource.com/chromium/src/+/main/docs/tab_helpers.md
class SyncSessionsRouterTabHelper
    : public content::WebContentsUserData<SyncSessionsRouterTabHelper>,
      public content::WebContentsObserver,
      public translate::TranslateDriver::LanguageDetectionObserver,
      public favicon::FaviconDriverObserver {
 public:
  SyncSessionsRouterTabHelper(const SyncSessionsRouterTabHelper&) = delete;
  SyncSessionsRouterTabHelper& operator=(const SyncSessionsRouterTabHelper&) =
      delete;

  ~SyncSessionsRouterTabHelper() override;

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
  void OnVisibilityChanged(content::Visibility visibility) override;

  // TranslateDriver::LanguageDetectionObserver implementation.
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
  const raw_ptr<SyncSessionsWebContentsRouter, DanglingUntriaged> router_;

  const raw_ptr<ChromeTranslateClient, AcrossTasksDanglingUntriaged>
      chrome_translate_client_;

  const raw_ptr<favicon::FaviconDriver, AcrossTasksDanglingUntriaged>
      favicon_driver_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sync_sessions

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_ROUTER_TAB_HELPER_H_
