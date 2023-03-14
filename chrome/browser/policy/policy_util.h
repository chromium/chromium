// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_UTIL_H_
#define CHROME_BROWSER_POLICY_POLICY_UTIL_H_

class GURL;
class PrefService;

namespace policy {

// Check if the origin provided by `url` is in the allowlist for a given
// policy-controlled feature by its `allowlist_pref_name`. The optional
// `always_allow_pref_name` can be used if there is a policy/pref that enables a
// feature for all URLs (e.g. AutoplayAllowed combines with AutoplayAllowlist).
bool IsOriginInAllowlist(const GURL& url,
                         const PrefService* prefs,
                         const char* allowlist_pref_name,
                         const char* always_allow_pref_name = nullptr);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_UTIL_H_
