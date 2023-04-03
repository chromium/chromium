// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_FEATURE_PROMO_HELPER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_FEATURE_PROMO_HELPER_H_

#include "content/public/browser/web_contents_observer.h"

class CustomizeChromeFeaturePromoHelper {
 public:
  virtual void RecordCustomizeChromeFeatureUsage(
      content::WebContents* web_contents);
  virtual void MaybeShowCustomizeChromeFeaturePromo(
      content::WebContents* web_contents);
  virtual void CloseCustomizeChromeFeaturePromo(
      content::WebContents* web_contents);

  virtual ~CustomizeChromeFeaturePromoHelper() = default;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_FEATURE_PROMO_HELPER_H_
