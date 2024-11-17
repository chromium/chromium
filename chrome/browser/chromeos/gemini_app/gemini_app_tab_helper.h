// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GEMINI_APP_GEMINI_APP_TAB_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_GEMINI_APP_GEMINI_APP_TAB_HELPER_H_

#include <map>

#include "base/auto_reset.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Helper which is attached to every browser tab in order to record visits to
// Gemini app related pages.
class GeminiAppTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<GeminiAppTabHelper> {
 public:
  // Enumerations of pages related to the Gemini app for which visits should
  // be recorded. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class Page {
    kMinValue = 0,
    kCongratulations = kMinValue,
    kOffer = 1,
    kTermsAndConditions = 2,
    kDebug = 3,
    kMaxValue = kDebug,
  };

  GeminiAppTabHelper(const GeminiAppTabHelper&) = delete;
  GeminiAppTabHelper& operator=(const GeminiAppTabHelper&) = delete;
  ~GeminiAppTabHelper() override;

  // Attaches a new instance to `web_contents` if and only if:
  // (a) the Gemini app preinstallation feature is enabled, and
  // (b) the specified `web_contents` is not off the record.
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  // Temporarily replaces the URLs of pages for which visits should be recorded
  // until the returned `base::AutoReset` is destroyed.
  [[nodiscard]] static base::AutoReset<std::map<uint64_t, Page>>
  SetPageUrlsForTesting(std::map<GURL, Page> page_urls);

 private:
  explicit GeminiAppTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<GeminiAppTabHelper>;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_CHROMEOS_GEMINI_APP_GEMINI_APP_TAB_HELPER_H_
