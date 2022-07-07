// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_UTILS_H_

#include <ostream>

#include "base/strings/string_piece_forward.h"

namespace base {
class TimeDelta;
}

// NOTE: We use this type as a bitfield, and will soon be logging it. Don't
// change the values or add additional members.
enum class CookieAccessType {
  kNone = 0,
  kRead = 1,
  kWrite = 2,
  kReadWrite = 3
};

base::StringPiece CookieAccessTypeToString(CookieAccessType type);

constexpr CookieAccessType operator|(CookieAccessType lhs,
                                     CookieAccessType rhs) {
  return static_cast<CookieAccessType>(static_cast<int>(lhs) |
                                       static_cast<int>(rhs));
}

// DIPSCookieMode:
enum class DIPSCookieMode {
  kStandard,
  kOffTheRecord,
  kBlock3PC,  // block third-party cookies
  kOffTheRecord_Block3PC
};

DIPSCookieMode GetDIPSCookieMode(bool is_otr, bool block_third_party_cookies);
base::StringPiece GetHistogramSuffix(DIPSCookieMode mode);
const char* DIPSCookieModeToString(DIPSCookieMode mode);
std::ostream& operator<<(std::ostream& os, DIPSCookieMode mode);

// DIPSRedirectType:
enum class DIPSRedirectType { kClient, kServer };

base::StringPiece GetHistogramPiece(DIPSRedirectType type);
const char* DIPSRedirectTypeToString(DIPSRedirectType type);
std::ostream& operator<<(std::ostream& os, DIPSRedirectType type);

// Return the number of seconds in `td`, clamped to [0, 10].
// i.e. 11 linearly-sized buckets.
int64_t BucketizeBounceDelay(base::TimeDelta delta);

#endif  // CHROME_BROWSER_DIPS_DIPS_UTILS_H_
