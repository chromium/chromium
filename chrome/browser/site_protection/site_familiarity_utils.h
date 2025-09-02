// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_

class PrefService;

namespace site_protection {

// Returns whether v8-optimizations are disabled on sites which are unfamiliar
// to the user. Site familiarity is computed using a heuristic based on the
// user's navigation history.
bool AreV8OptimizationsDisabledOnUnfamiliarSites(const PrefService& prefs);

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_
