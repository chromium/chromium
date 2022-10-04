// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_UTILS_H_

#include <ostream>

#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

// A single cookie-accessing operation (either read or write). Not to be
// confused with CookieAccessType, which can also represent no access or both
// read+write.
using CookieOperation = network::mojom::CookieAccessDetails::Type;

// NOTE: We use this type as a bitfield, and will soon be logging it. Don't
// change the values or add additional members.
enum class CookieAccessType {
  kUnknown = -1,
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

// StateValue:
struct StateValue {
  absl::optional<base::Time> first_site_storage_time;
  absl::optional<base::Time> last_site_storage_time;
  absl::optional<base::Time> first_user_interaction_time;
  absl::optional<base::Time> last_user_interaction_time;
};

inline bool operator==(const StateValue& lhs, const StateValue& rhs) {
  return (lhs.first_site_storage_time == rhs.first_site_storage_time) &&
         (lhs.last_site_storage_time == rhs.last_site_storage_time) &&
         (lhs.first_user_interaction_time == rhs.first_user_interaction_time) &&
         (lhs.last_user_interaction_time == rhs.last_user_interaction_time);
}

// Return the number of seconds in `td`, clamped to [0, 10].
// i.e. 11 linearly-sized buckets.
int64_t BucketizeBounceDelay(base::TimeDelta delta);

// Returns an opaque value representing the "privacy boundary" that the URL
// belongs to. Currently returns eTLD+1, but this is an implementation detail
// and may change (e.g. after adding support for First-Party Sets).
std::string GetSiteForDIPS(const GURL& url);

#endif  // CHROME_BROWSER_DIPS_DIPS_UTILS_H_
