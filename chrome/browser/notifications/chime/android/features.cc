// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chime/android/features.h"

namespace notifications {
namespace features {

BASE_FEATURE(kUseChimeAndroidSdk,
             "UseChimeAndroidSdk",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace switches {
const char kDebugChimeNotification[] = "debug-chime-notification";
}  // namespace switches

}  // namespace notifications
