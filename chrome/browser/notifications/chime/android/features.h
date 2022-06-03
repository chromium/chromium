// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_CHIME_ANDROID_FEATURES_H_
#define CHROME_BROWSER_NOTIFICATIONS_CHIME_ANDROID_FEATURES_H_

#include "base/feature_list.h"

namespace notifications {
namespace features {

// The feature flag to determine whether to use Chime Android SDK.
extern const base::Feature kUseChimeAndroidSdk;

}  // namespace features

namespace switches {
// Debug flag to show Chime notifications.
extern const char kDebugChimeNotification[];
}  // namespace switches
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_CHIME_ANDROID_FEATURES_H_
