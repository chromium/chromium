// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Feature for session-only data deletion on startup.

#include "chrome/browser/browser_switcher/browser_switcher_features.h"

// A kill-switch for the <open-in>None behavior change made in M99, in
// IeemSitelistParser. This makes it easy to revert the change if we break
// a customer.
//
// TODO(crbug.com/40812726): Remove this flag once we're confident this
// doesn't break customers.
BASE_FEATURE(kBrowserSwitcherNoneIsGreylist,
             "BrowserSwitcherNoneIsGreylist",
             base::FEATURE_ENABLED_BY_DEFAULT);
