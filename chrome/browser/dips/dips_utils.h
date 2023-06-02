// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_UTILS_H_

#include <ostream>

#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/navigation_handle.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class ProfileSelections;

namespace base {
class TimeDelta;
}

namespace content {
class BrowserContext;
}

// A single cookie-accessing operation (either read or write). Not to be
// confused with SiteDataAccessType, which can also represent no access or both
// read+write.
using CookieOperation = network::mojom::CookieAccessDetails::Type;
inline CookieOperation ToCookieOperation(content_settings::AccessType type) {
  switch (type) {
    case content_settings::AccessType::kRead:
    case content_settings::AccessType::kWrite:
    case content_settings::AccessType::kUnknown:
      return CookieOperation::kChange;
  }
}

// The filename for the DIPS database.
const base::FilePath::CharType kDIPSFilename[] = FILE_PATH_LITERAL("DIPS");

// The FilePath for the ON-DISK DIPSDatabase associated with a BrowserContext,
// if one exists.
// NOTE: This returns the same value regardless of if there is actually a
// persisted DIPSDatabase for the BrowserContext or not.
base::FilePath GetDIPSFilePath(content::BrowserContext* context);

// The ProfileSelections used to dictate when the DIPSService should be created,
// if `dips::kFeature` is enabled, and when the DIPSCleanupService should be
// created, if `dips::kFeature` is NOT enabled.
ProfileSelections GetHumanProfileSelections();

// SiteDataAccessType:
// NOTE: We use this type as a bitfield, and will soon be logging it. Don't
// change the values or add additional members.
enum class SiteDataAccessType {
  kUnknown = -1,
  kNone = 0,
  kRead = 1,
  kWrite = 2,
  kReadWrite = 3
};
inline SiteDataAccessType ToSiteDataAccessType(CookieOperation op) {
  return (op == CookieOperation::kChange ? SiteDataAccessType::kWrite
                                         : SiteDataAccessType::kRead);
}
base::StringPiece SiteDataAccessTypeToString(SiteDataAccessType type);
std::ostream& operator<<(std::ostream& os, SiteDataAccessType access_type);

constexpr SiteDataAccessType operator|(SiteDataAccessType lhs,
                                       SiteDataAccessType rhs) {
  return static_cast<SiteDataAccessType>(static_cast<int>(lhs) |
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

using TimestampRange = absl::optional<std::pair<base::Time, base::Time>>;
// Expand the range to include `time` if necessary. Returns true iff the range
// was modified.
bool UpdateTimestampRange(TimestampRange& range, base::Time time);
// Checks that `this` range is either null or falls within `other`.
bool IsNullOrWithin(const TimestampRange& inner, const TimestampRange& outer);

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

enum class DIPSTriggeringAction { kNone, kStorage, kBounce, kStatefulBounce };

// Return the number of seconds in `td`, clamped to [0, 10].
// i.e. 11 linearly-sized buckets.
int64_t BucketizeBounceDelay(base::TimeDelta delta);

// Returns an opaque value representing the "privacy boundary" that the URL
// belongs to. Currently returns eTLD+1, but this is an implementation detail
// and may change.
std::string GetSiteForDIPS(const GURL& url);

// Returns `True` iff the `navigation_handle` represents a navigation happening
// in an iframe of the primary frame tree.
inline bool IsInPrimaryPageIFrame(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->GetParentFrame()
             ? navigation_handle->GetParentFrame()->GetPage().IsPrimary()
             : false;
}

// Returns `True` iff both urls return a similar outcome off of
// `GetSiteForDIPS()`.
inline bool IsSameSiteForDIPS(const GURL& url1, const GURL& url2) {
  return GetSiteForDIPS(url1) == GetSiteForDIPS(url2);
}

// Returns `True` iff the `navigation_handle` represents a navigation happening
// in any frame of the primary page.
// NOTE: This does not include fenced frames.
inline bool IsInPrimaryPage(content::NavigationHandle* navigation_handle) {
  return navigation_handle->GetParentFrame()
             ? navigation_handle->GetParentFrame()->GetPage().IsPrimary()
             : navigation_handle->IsInPrimaryMainFrame();
}

// Returns `True` iff the 'rfh' represents a frame in the primary page.
inline bool IsInPrimaryPage(content::RenderFrameHost* rfh) {
  return rfh->GetPage().IsPrimary();
}

enum class DIPSRecordedEvent {
  kStorage,
  kInteraction,
};

// RedirectCategory is basically the cross-product of SiteDataAccessType and a
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

// DIPSErrorCode is used in UMA enum histograms to monitor certain errors and
// verify that they are being fixed.
//
// When adding an error to this enum, update the DIPSErrorCode enum in
// tools/metrics/histograms/enums.xml as well.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DIPSErrorCode {
  kRead_None = 0,
  kRead_OpenEndedRange_NullStart = 1,
  kRead_OpenEndedRange_NullEnd = 2,
  kRead_BounceTimesIsntSupersetOfStatefulBounces = 3,
  kMaxValue = kRead_BounceTimesIsntSupersetOfStatefulBounces,
};

// DIPSDeletionAction is used in UMA enum histograms to record the actual
// deletion action taken on DIPS-eligible (incidental) site.
//
// When adding an action to this enum, update the DIPSDeletionAction enum in
// tools/metrics/histograms/enums.xml as well.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DIPSDeletionAction {
  kDisallowed = 0,
  kExceptedAs1p = 1,
  kExceptedAs3p = 2,
  kEnforced = 3,
  kIgnored = 4,
  kMaxValue = kIgnored,
};

#endif  // CHROME_BROWSER_DIPS_DIPS_UTILS_H_
