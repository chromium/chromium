// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_CHROME_SAFE_BROWSING_HATS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_CHROME_SAFE_BROWSING_HATS_DELEGATE_ANDROID_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {

class ChromeSafeBrowsingHatsDelegateAndroid : public SafeBrowsingHatsDelegate {
 public:
  explicit ChromeSafeBrowsingHatsDelegateAndroid(Profile* profile);
  ~ChromeSafeBrowsingHatsDelegateAndroid() override;

  // SafeBrowsingHatsDelegate:
  void LaunchRedWarningSurvey(SurveyStringData product_specific_string_data,
                              SurveyBitsData product_specific_bits_data,
                              bool is_tab_closed) override;

  void SetReferringAppNameForTesting(
      std::optional<std::string> referring_app_name) {
    referring_app_name_for_testing_ = std::move(referring_app_name);
  }

 private:
  void LaunchRedWarningSurveyInternal(
      SurveyStringData product_specific_string_data,
      SurveyBitsData product_specific_bits_data,
      bool is_tab_closed);

  internal::ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents);

  // raw_ptr is safe because this object is owned by keyed service, which will
  // be destroyed before the profile is destroyed.
  const raw_ptr<Profile> profile_;
  std::optional<std::string> referring_app_name_for_testing_;
  base::WeakPtrFactory<ChromeSafeBrowsingHatsDelegateAndroid> weak_ptr_factory_{
      this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_CHROME_SAFE_BROWSING_HATS_DELEGATE_ANDROID_H_
