// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_CHROME_SAFE_BROWSING_HATS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_CHROME_SAFE_BROWSING_HATS_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"

class Profile;

namespace safe_browsing {

class ChromeSafeBrowsingHatsDelegateAndroid : public SafeBrowsingHatsDelegate {
 public:
  explicit ChromeSafeBrowsingHatsDelegateAndroid(Profile* profile);
  ~ChromeSafeBrowsingHatsDelegateAndroid() override = default;

  // SafeBrowsingHatsDelegate:
  void LaunchRedWarningSurvey(
      SurveyStringData product_specific_string_data,
      SurveyBitsData product_specific_bits_data) override;

 private:
  // raw_ptr is safe because this object is owned by keyed service, which will
  // be destroyed before the profile is destroyed.
  const raw_ptr<Profile> profile_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_CHROME_SAFE_BROWSING_HATS_DELEGATE_ANDROID_H_
