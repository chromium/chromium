// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "content/public/browser/global_routing_id.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/safe_browsing/core/browser/referring_app_info.h"
#endif

namespace safe_browsing {

// Delegate class which implements chrome specific bits for configuring
// the ClientSideDetectionHost class.
class ChromeClientSideDetectionHostDelegate
    : public ClientSideDetectionHost::Delegate {
 public:
  static std::unique_ptr<ClientSideDetectionHost> CreateHost(
      content::WebContents* tab);

  explicit ChromeClientSideDetectionHostDelegate(
      content::WebContents* web_contents);

  ChromeClientSideDetectionHostDelegate(
      const ChromeClientSideDetectionHostDelegate&) = delete;
  ChromeClientSideDetectionHostDelegate& operator=(
      const ChromeClientSideDetectionHostDelegate&) = delete;

  ~ChromeClientSideDetectionHostDelegate() override;

  // ClientSideDetectionHost::Delegate implementation.
  bool HasSafeBrowsingUserInteractionObserver() override;
  PrefService* GetPrefs() override;
  scoped_refptr<SafeBrowsingDatabaseManager> GetSafeBrowsingDBManager()
      override;
  scoped_refptr<BaseUIManager> GetSafeBrowsingUIManager() override;
  base::WeakPtr<ClientSideDetectionService> GetClientSideDetectionService()
      override;
  void AddReferrerChain(ClientPhishingRequest* verdict,
                        GURL current_url,
                        const content::GlobalRenderFrameHostId&
                            current_outermost_main_frame_id) override;
  VerdictCacheManager* GetCacheManager() override;
  ChromeUserPopulation GetUserPopulation() override;
  void GetInnerText(HostInnerTextCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  internal::ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents) override;
#endif

  void SetNavigationObserverManagerForTesting(
      SafeBrowsingNavigationObserverManager* navigation_observer_manager) {
    observer_manager_for_testing_ = navigation_observer_manager;
  }

 protected:
  SafeBrowsingNavigationObserverManager*
  GetSafeBrowsingNavigationObserverManager();
  size_t CountOfRecentNavigationsToAppend(
      SafeBrowsingNavigationObserverManager::AttributionResult result);

 private:
  void OnInnerTextResult(
      HostInnerTextCallback callback,
      std::unique_ptr<content_extraction::InnerTextResult> result);
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<SafeBrowsingNavigationObserverManager> observer_manager_for_testing_ =
      nullptr;
  base::WeakPtrFactory<ChromeClientSideDetectionHostDelegate> weak_factory_{
      this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
