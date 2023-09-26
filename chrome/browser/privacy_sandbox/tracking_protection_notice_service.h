
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Profile;

namespace privacy_sandbox {

// A service which contains the logic tracking some user interactions with the
// browser, in order to determine when the best time is to Show the Onboarding
// Notice, then actually displays it.
// If the profile is not to be shown the notice at all due to ineligibility,
// then this service doesn't observe anything (Except the
// TrackingProtectionOnboarding Service).
// We are observing two different types of interactions:
//    1. Using the TabStripModelObserver: All updates to the tabs: This allows
//    us to show/hide the notice on all tabs (including tabs that we started
//    observing newly created webcontents) after the user selects a new one.
//
//    2. Using the WebContentsObserver: Navigation updates to the active
//    webcontent: This allows us to show/hide the notice based on the
//    navigation, in case the user doesn't update the tab, but only its
//    webcontent through navigation.
class TrackingProtectionNoticeService
    : public KeyedService,
      public TabStripModelObserver,
      public BrowserTabStripTrackerDelegate,
      public TrackingProtectionOnboarding::Observer {
 public:
  TrackingProtectionNoticeService(
      Profile* profile,
      TrackingProtectionOnboarding* onboarding_service);
  ~TrackingProtectionNoticeService() override;

  class TabHelper : public content::WebContentsObserver,
                    public content::WebContentsUserData<TabHelper> {
   public:
    TabHelper(const TabHelper&) = delete;
    TabHelper& operator=(const TabHelper&) = delete;
    ~TabHelper() override;

    // Static method that tells us if the helper is needed. This is to be
    // checked before creating the helper so we don't unnecessarily create one
    // for every WebContent.
    static bool IsHelperNeeded(Profile* profile);

   private:
    friend class content::WebContentsUserData<TabHelper>;
    explicit TabHelper(content::WebContents* web_contents);

    // contents::WebContentsObserver:
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;

    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };

 private:
  // Indicates if the notice is needed to be displayed.
  bool IsNoticeNeeded();

  // Assumes this is a time to show the user the onboarding Notice. This
  // method will attempt do so.
  void MaybeUpdateNoticeVisibility(content::WebContents* web_content);

  // Fires when the Notice is closed (for any reason).
  void OnNoticeClosed(base::Time showed_when,
                      user_education::FeaturePromoController* promo_controller);

  // This is called internally when the service should start observing the tab
  // strip model across all eligible browsers. Browser eligibility is determined
  // by the  "ShouldTrackBrowser" below.
  void InitializeTabStripTracker();

  // This is called internally when the service should no longer observe changes
  // to the tab strip model.
  void ResetTabStripTracker();

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserTabStripTrackerDelegate
  bool ShouldTrackBrowser(Browser* browser) override;

  // TrackingProtectionOnboarding::Observer
  void OnShouldShowNoticeUpdated() override;

  raw_ptr<Profile> profile_;
  std::unique_ptr<BrowserTabStripTracker> tab_strip_tracker_;
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
  base::ScopedObservation<TrackingProtectionOnboarding,
                          TrackingProtectionOnboarding::Observer>
      onboarding_observation_{this};
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_
