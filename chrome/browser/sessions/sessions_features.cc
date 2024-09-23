// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Feature for session-only data deletion on startup.

#include "chrome/browser/sessions/sessions_features.h"

BASE_FEATURE(kDeleteSessionOnlyDataOnStartup,
             "DeleteSessionOnlyDataOnStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeleteStaleSessionCookiesOnStartup,
             "DeleteStaleSessionCookiesOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);
