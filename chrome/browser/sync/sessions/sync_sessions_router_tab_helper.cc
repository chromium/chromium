// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/language/core/common/language_experiments.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync/base/features.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/image/image_skia.h"

namespace sync_sessions {

SyncSessionsRouterTabHelper::SyncSessionsRouterTabHelper(
    content::WebContents* web_contents,
    SyncSessionsWebContentsRouter* router)
    : content::WebContentsUserData<SyncSessionsRouterTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents),
      router_(router),
      chrome_translate_client_(
          ChromeTranslateClient::FromWebContents(web_contents)),
      favicon_driver_(
          favicon::ContentFaviconDriver::FromWebContents(web_contents)) {
  // A translate client is not always attached to web contents (e.g. tests).
  if (chrome_translate_client_) {
    chrome_translate_client_->GetTranslateDriver()
        ->AddLanguageDetectionObserver(this);
  }

  if (favicon_driver_) {
    favicon_driver_->AddObserver(this);
  }
}

SyncSessionsRouterTabHelper::~SyncSessionsRouterTabHelper() = default;

void SyncSessionsRouterTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && navigation_handle->IsInPrimaryMainFrame()) {
    NotifyRouter();
  }
}

void SyncSessionsRouterTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::WebContentsDestroyed() {
  NotifyRouter();
  if (chrome_translate_client_) {
    chrome_translate_client_->GetTranslateDriver()
        ->RemoveLanguageDetectionObserver(this);
  }
  if (favicon_driver_) {
    favicon_driver_->RemoveObserver(this);
  }
}

void SyncSessionsRouterTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Only notify when the primary main frame finishes loading.
  if (render_frame_host && render_frame_host->IsInPrimaryMainFrame()) {
    NotifyRouter(true);
  }
}

void SyncSessionsRouterTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  // TODO(crbug.com/40649749): This is a relic from when we actually did change
  // something about the tab here. It should be safe to remove now.
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  // Only notify a notification when the tab becomes visible. This is necessary
  // to sync the last active time field.
  if (visibility == content::Visibility::VISIBLE) {
    NotifyRouter();
  }
}

void SyncSessionsRouterTabHelper::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::NotifyRouter(bool page_load_completed) {
  if (router_) {
    router_->NotifyTabModified(web_contents(), page_load_completed);
  }
}

void SyncSessionsRouterTabHelper::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  if (icon_url_changed) {
    NotifyRouter();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SyncSessionsRouterTabHelper);

}  // namespace sync_sessions
