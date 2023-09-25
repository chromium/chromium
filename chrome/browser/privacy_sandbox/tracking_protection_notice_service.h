
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
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
class TrackingProtectionNoticeService : public KeyedService {
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

    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };

 private:
  // Indicates if the notice is needed to be displayed.
  bool IsNoticeNeeded();

  raw_ptr<Profile> profile_;
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_NOTICE_SERVICE_H_
