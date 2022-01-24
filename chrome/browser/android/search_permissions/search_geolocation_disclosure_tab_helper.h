// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_GEOLOCATION_DISCLOSURE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_GEOLOCATION_DISCLOSURE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Tab helper to show the search geolocation disclosure.
class SearchGeolocationDisclosureTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          SearchGeolocationDisclosureTabHelper> {
 public:
  SearchGeolocationDisclosureTabHelper(
      const SearchGeolocationDisclosureTabHelper&) = delete;
  SearchGeolocationDisclosureTabHelper& operator=(
      const SearchGeolocationDisclosureTabHelper&) = delete;

  ~SearchGeolocationDisclosureTabHelper() override;

  // content::WebContentsObserver overrides.
  void PrimaryPageChanged(content::Page& page) override;

  void MaybeShowDisclosureForAPIAccess(content::RenderFrameHost* rfh,
                                       const GURL& requesting_origin);

  static void ResetDisclosure(Profile* profile);

  // Testing methods to ensure the disclosure is reset when it should be.
  static void FakeShowingDisclosureForTests(Profile* profile);
  static bool IsDisclosureResetForTests(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  explicit SearchGeolocationDisclosureTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<
      SearchGeolocationDisclosureTabHelper>;

  void MaybeShowDisclosureForValidUrl(content::RenderFrameHost* rfh,
                                      const GURL& gurl);

  // Determines if the disclosure should be shown for the URL when a navigation
  // to the URL occurs. This is the case whenever the URL is a result of an
  // omnibox search, as it will result in X-Geo headers being sent.
  bool ShouldShowDisclosureForNavigation(const GURL& gurl);

  // Determine if the disclosure should be shown for the URL when a page on the
  // URL uses the geolocation API. This is the case if the url's access to the
  // geolocation API is allowed due to the geolocation DSE setting.
  bool ShouldShowDisclosureForAPIAccess(const GURL& gurl);

  // Record metrics, once per client, of the permission state before and after
  // the disclosure has been shown.
  void RecordPreDisclosureMetrics(const GURL& gurl);
  void RecordPostDisclosureMetrics(const GURL& gurl);
  Profile* GetProfile();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_GEOLOCATION_DISCLOSURE_TAB_HELPER_H_
