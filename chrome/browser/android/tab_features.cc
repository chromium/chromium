// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_features.h"

#include "chrome/browser/privacy_sandbox/incognito/privacy_sandbox_incognito_tab_observer.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/metrics/content/dwa_web_contents_observer.h"

namespace tabs {

TabFeatures::TabFeatures(content::WebContents* web_contents, Profile* profile) {
  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          web_contents,
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(web_contents),
          favicon::ContentFaviconDriver::FromWebContents(web_contents));

  dwa_web_contents_observer_ =
      std::make_unique<metrics::DwaWebContentsObserver>(web_contents);

  privacy_sandbox_incognito_tab_observer_ =
      std::make_unique<privacy_sandbox::PrivacySandboxIncognitoTabObserver>(
          web_contents);
}

TabFeatures::~TabFeatures() = default;

}  // namespace tabs
