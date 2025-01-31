// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_tab_observer_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/buildflags.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#endif

namespace safe_browsing {

ChromeSafeBrowsingTabObserverDelegate::ChromeSafeBrowsingTabObserverDelegate() =
    default;
ChromeSafeBrowsingTabObserverDelegate::
    ~ChromeSafeBrowsingTabObserverDelegate() = default;

PrefService* ChromeSafeBrowsingTabObserverDelegate::GetPrefs(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->GetPrefs();
}

ClientSideDetectionService*
ChromeSafeBrowsingTabObserverDelegate::GetClientSideDetectionServiceIfExists(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  return ClientSideDetectionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
#else
  return nullptr;
#endif
}

bool ChromeSafeBrowsingTabObserverDelegate::DoesSafeBrowsingServiceExist() {
  return g_browser_process->safe_browsing_service();
}

std::unique_ptr<ClientSideDetectionHost>
ChromeSafeBrowsingTabObserverDelegate::CreateClientSideDetectionHost(
    content::WebContents* web_contents) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  return ChromeClientSideDetectionHostDelegate::CreateHost(web_contents);
#else
  return nullptr;
#endif
}

}  // namespace safe_browsing
