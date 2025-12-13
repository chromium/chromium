// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_FEATURE_PROMO_HELPER_NEW_TAB_PAGE_FEATURE_PROMO_HELPER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_FEATURE_PROMO_HELPER_NEW_TAB_PAGE_FEATURE_PROMO_HELPER_H_

#include "base/feature_list.h"
#include "content/public/browser/web_contents_observer.h"

namespace user_education {
struct FeaturePromoParams;
}  // namespace user_education

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NTPCustomizeChromePromoEligibility)
enum class NTPCustomizeChromePromoEligibility {
  kCanShowPromo = 0,
  kReachedTotalMaxCountAlready = 1,
  kReachedSessionMaxCountAlready = 2,
  kChromeCustomizedAlready = 3,
  kCustomizeChromeClosedExplicitlyByUser = 4,
  kCustomizeChromeOpenedByUser = 5,
  kFeatureConfigMismatch = 6,
  kMaxValue = kFeatureConfigMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NTPCustomizeChromePromoEligibility)

class NewTabPageFeaturePromoHelper {
 public:
  virtual void RecordPromoFeatureUsageAndClosePromo(
      const base::Feature& iph_feature,
      content::WebContents* web_contents);
  virtual void SetDefaultSearchProviderIsGoogleForTesting(bool value);
  virtual bool DefaultSearchProviderIsGoogle(Profile* profile);
  virtual void MaybeShowFeaturePromo(user_education::FeaturePromoParams params,
                                     content::WebContents* web_contents);
  virtual bool IsSigninModalDialogOpen(content::WebContents* web_contents);
  virtual void MaybeTriggerAutomaticCustomizeChromePromo(
      content::WebContents* web_contents);

  virtual ~NewTabPageFeaturePromoHelper() = default;

 private:
  std::optional<bool> default_search_provider_is_google_ = std::nullopt;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_FEATURE_PROMO_HELPER_NEW_TAB_PAGE_FEATURE_PROMO_HELPER_H_
