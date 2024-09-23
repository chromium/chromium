// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSIONS_FEATURES_H_
#define CHROME_BROWSER_SESSIONS_SESSIONS_FEATURES_H_

#include "base/feature_list.h"

BASE_DECLARE_FEATURE(kDeleteSessionOnlyDataOnStartup);

// Clears session cookies last accessed/modified more than 7 days ago on startup
// even when session restore is enabled.
// See crbug.com/40285083 for more info.
BASE_DECLARE_FEATURE(kDeleteStaleSessionCookiesOnStartup);

#endif  // CHROME_BROWSER_SESSIONS_SESSIONS_FEATURES_H_
