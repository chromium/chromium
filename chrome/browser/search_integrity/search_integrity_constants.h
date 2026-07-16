// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_CONSTANTS_H_
#define CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_CONSTANTS_H_

#include <stddef.h>

namespace search_integrity {

// The number of hash functions and bits for the bloom filter. These values are
// chosen based on the expected number of search engine entries (~200)
// and a desired false-positive rate of ~1%.
inline constexpr int kNumHashFunctions = 10;
inline constexpr int kNumBits = 2875;

// The name of the bloom filter file stored in the user's profile directory.
inline constexpr char kSearchEngineAllowlistFileName[] = "engine_allowlist.bf";

// The minimum word length to consider when comparing search engine names.
inline constexpr size_t kMinWordLength = 3;

// The number of hex escapes in a URL host to consider it obfuscated.
inline constexpr int kObfuscatedUrlPercentThreshold = 3;

}  // namespace search_integrity

#endif  // CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_CONSTANTS_H_
