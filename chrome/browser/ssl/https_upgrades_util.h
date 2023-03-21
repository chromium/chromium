// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_UPGRADES_UTIL_H_
#define CHROME_BROWSER_SSL_HTTPS_UPGRADES_UTIL_H_

#include "base/values.h"
#include "url/gurl.h"

// Helper for applying the HttpAllowlist enterprise policy. Checks if the
// hostname of `url` matches any of the hostnames or hostname patterns in the
// `allowed_hosts` list. Does not allow blanket host wildcards (i.e., "*" which
// matches all hosts), but does allow partial domain wildcards (e.g.,
// "[*.]example.com"). Entries in `allowed_hosts` should follow the rules in
// https://chromeenterprise.google/policies/url-patterns/ (or they'll be
// ignored).
bool IsHostnameInAllowlist(const GURL& url,
                           const base::Value::List& allowed_hosts);

#endif  // CHROME_BROWSER_SSL_HTTPS_UPGRADES_UTIL_H_
