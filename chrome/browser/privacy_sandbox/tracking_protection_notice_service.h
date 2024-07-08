// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
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

  // KeyedService:
  void Shutdown() override;

  // Enum class used for recording histogram events.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class TrackingProtectionNoticeServiceEvent {
    kPromoPreviouslyDismissed = 0,
    kActiveTabChanged = 1,
    kNavigationFinished = 2,
    kUpdateNoticeVisibility = 3,
    kBrowserTypeNonNormal = 4,
    kNoticeShowingButShouldnt = 5,
    kInactiveWebcontentUpdated = 6,
    kLocationIconNonSecure = 7,
    kLocationIconNonVisible = 8,
    kNoticeAlreadyShowing = 9,
    kNoticeRequestedAndShown = 10,
    kNoticeRequestedButNotShown = 11,
    kMaxValue = kNoticeRequestedButNotShown,
  };

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
  class NoticeBehavior;

  // IPH Based Tracking Protection Notice, in charge of showing/hiding the IPH
  // Promo based on page eligibility and user navigation.
  class BaseIPHNotice {
   public:
    BaseIPHNotice(Profile* profile,
                  TrackingProtectionOnboarding* onboarding_service,
                  TrackingProtectionNoticeService* notice_service);
    ~BaseIPHNotice();

    void MaybeUpdateNoticeVisibility(content::WebContents* web_content);

    TrackingProtectionNoticeService* notice_service() const {
      return notice_service_;
    }

    // Fires when the notice is closed (For any reason)
    void OnNoticeClosed(
        base::Time showed_when,
        user_education::FeaturePromoController* promo_controller);

    const base::Feature& GetIPHFeature();

   private:
    void MaybeInitNoticeBehavior();
    bool IsLocationBarEligible(Browser* browser);
    TrackingProtectionOnboarding::NoticeType GetNoticeType();

    NoticeBehavior* notice_behavior() const { return notice_behavior_.get(); }

    raw_ptr<Profile> profile_;
    // Behavior to be loaded lazily, when we're ready to execute it.
    std::unique_ptr<NoticeBehavior> notice_behavior_;
    std::optional<TrackingProtectionOnboarding::NoticeType> notice_type_;
    raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
    raw_ptr<TrackingProtectionNoticeService> notice_service_;
  };

  class NoticeBehavior {
   public:
    explicit NoticeBehavior(BaseIPHNotice* notice);
    virtual ~NoticeBehavior();
    virtual bool WasPromoPreviouslyDismissed(Browser* browser) = 0;
    virtual bool MaybeShowPromo(Browser* browser) = 0;
    virtual bool IsPromoShowing(Browser* browser) = 0;
    virtual bool HidePromo(Browser* browser) = 0;

   protected:
    raw_ptr<BaseIPHNotice> notice_;
  };

  // The Silent Notice is shown to the user without any visual indication.
  // It is only used for control groups.
  class SilentNotice : public NoticeBehavior {
   public:
    using NoticeBehavior::NoticeBehavior;
    bool WasPromoPreviouslyDismissed(Browser* browser) override;
    bool MaybeShowPromo(Browser* browser) override;
    bool IsPromoShowing(Browser* browser) override;
    bool HidePromo(Browser* browser) override;
  };

  // The Visible Notice is shown to the user as a IPH promo.
  // It is only used for the main onboarding experience.
  class VisibleNotice : public NoticeBehavior {
   public:
    using NoticeBehavior::NoticeBehavior;
    bool WasPromoPreviouslyDismissed(Browser* browser) override;
    bool MaybeShowPromo(Browser* browser) override;
    bool IsPromoShowing(Browser* browser) override;
    bool HidePromo(Browser* browser) override;
  };

  // Indicates if the notice is needed to be displayed.
  bool IsNoticeNeeded();

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
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
  std::unique_ptr<BaseIPHNotice> tracking_protection_notice_;
  std::unique_ptr<BrowserTabStripTracker> tab_strip_tracker_;
  base::ScopedObservation<TrackingProtectionOnboarding,
                          TrackingProtectionOnboarding::Observer>
      onboarding_observation_{this};
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_
