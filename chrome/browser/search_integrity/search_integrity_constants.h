// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_CONSTANTS_H_
#define CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_CONSTANTS_H_

#include <stddef.h>

namespace search_integrity {

// The minimum word length to consider when comparing search engine names.
inline constexpr size_t kMinWordLength = 3;

// The number of hex escapes in a URL host to consider it obfuscated.
inline constexpr int kObfuscatedUrlPercentThreshold = 3;

}  // namespace search_integrity

#endif  // CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_CONSTANTS_H_
