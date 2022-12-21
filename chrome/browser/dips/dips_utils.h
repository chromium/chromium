// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_UTILS_H_

#include <ostream>

#include "base/files/file_path.h"
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

// Constants:
// The filename for the DIPS database.
const base::FilePath::CharType kDIPSFilename[] = FILE_PATH_LITERAL("DIPS");

// CookieAccessType:
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

// DIPSEventRemovalType:
// NOTE: We use this type as a bitfield don't change existing values other than
// kAll, which should be updated to include any new fields.
enum class DIPSEventRemovalType {
  kNone = 0,
  kHistory = 1 << 0,
  kStorage = 1 << 1,
  // kAll is intended to cover all the above fields.
  kAll = kHistory | kStorage
};

constexpr DIPSEventRemovalType operator|(DIPSEventRemovalType lhs,
                                         DIPSEventRemovalType rhs) {
  return static_cast<DIPSEventRemovalType>(static_cast<int>(lhs) |
                                           static_cast<int>(rhs));
}

constexpr DIPSEventRemovalType operator&(DIPSEventRemovalType lhs,
                                         DIPSEventRemovalType rhs) {
  return static_cast<DIPSEventRemovalType>(static_cast<int>(lhs) &
                                           static_cast<int>(rhs));
}

constexpr DIPSEventRemovalType& operator|=(DIPSEventRemovalType& lhs,
                                           DIPSEventRemovalType rhs) {
  return lhs = lhs | rhs;
}

constexpr DIPSEventRemovalType& operator&=(DIPSEventRemovalType& lhs,
                                           DIPSEventRemovalType rhs) {
  return lhs = lhs & rhs;
}

// DIPSRedirectType:
enum class DIPSRedirectType { kClient, kServer };

base::StringPiece GetHistogramPiece(DIPSRedirectType type);
const char* DIPSRedirectTypeToString(DIPSRedirectType type);
std::ostream& operator<<(std::ostream& os, DIPSRedirectType type);

struct TimestampRange {
  absl::optional<base::Time> first;
  absl::optional<base::Time> last;

  // Expand the range to include `time` if necessary. Returns true iff the range
  // was modified.
  bool Update(base::Time time);

  // Checks that `this` range is either null or falls within `other`.
  bool IsNullOrWithin(TimestampRange other) const;
};

inline bool operator==(const TimestampRange& lhs, const TimestampRange& rhs) {
  return std::tie(lhs.first, lhs.last) == std::tie(rhs.first, rhs.last);
}

std::ostream& operator<<(std::ostream& os, TimestampRange type);

// StateValue:
struct StateValue {
  TimestampRange site_storage_times;
  TimestampRange user_interaction_times;
  TimestampRange stateful_bounce_times;
  TimestampRange bounce_times;
};

inline bool operator==(const StateValue& lhs, const StateValue& rhs) {
  return std::tie(lhs.site_storage_times, lhs.user_interaction_times,
                  lhs.stateful_bounce_times, lhs.bounce_times) ==
         std::tie(rhs.site_storage_times, rhs.user_interaction_times,
                  rhs.stateful_bounce_times, rhs.bounce_times);
}

enum class DIPSTriggeringAction { kStorage, kBounce, kStatefulBounce };

// Return the number of seconds in `td`, clamped to [0, 10].
// i.e. 11 linearly-sized buckets.
int64_t BucketizeBounceDelay(base::TimeDelta delta);

// Returns an opaque value representing the "privacy boundary" that the URL
// belongs to. Currently returns eTLD+1, but this is an implementation detail
// and may change.
std::string GetSiteForDIPS(const GURL& url);

enum class DIPSRecordedEvent {
  kStorage,
  kInteraction,
};

// RedirectCategory is basically the cross-product of CookieAccessType and a
// boolean value indicating site engagement. It's used in UMA enum histograms.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RedirectCategory {
  kNoCookies_NoEngagement = 0,
  kReadCookies_NoEngagement = 1,
  kWriteCookies_NoEngagement = 2,
  kReadWriteCookies_NoEngagement = 3,
  kNoCookies_HasEngagement = 4,
  kReadCookies_HasEngagement = 5,
  kWriteCookies_HasEngagement = 6,
  kReadWriteCookies_HasEngagement = 7,
  kUnknownCookies_NoEngagement = 8,
  kUnknownCookies_HasEngagement = 9,
  kMaxValue = kUnknownCookies_HasEngagement,
};

#endif  // CHROME_BROWSER_DIPS_DIPS_UTILS_H_
