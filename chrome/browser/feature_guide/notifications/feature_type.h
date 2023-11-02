// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_TYPE_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_TYPE_H_

#include "base/logging.h"

namespace feature_guide {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.feature_guide.notifications)
enum class FeatureType {
  kTest = -1,
  kInvalid = 0,
  kDefaultBrowser = 1,
  kSignIn = 2,
  kIncognitoTab = 3,
  kNTPSuggestionCard = 4,
  kVoiceSearch = 5,
  kMaxValue = kVoiceSearch,
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_TYPE_H_
