// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_UTILS_H_

#include <optional>
#include <ostream>
#include <string_view>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

// A single cookie-accessing operation (either read or write). Not to be
// confused with SiteDataAccessType, which can also represent no access or both
// read+write.
using CookieOperation = network::mojom::CookieAccessDetails::Type;

// The filename for the DIPS database.
const base::FilePath::CharType kDIPSFilename[] = FILE_PATH_LITERAL("DIPS");

// The FilePath for the ON-DISK DIPSDatabase associated with a BrowserContext,
// if one exists.
// NOTE: This returns the same value regardless of if there is actually a
// persisted DIPSDatabase for the BrowserContext or not.
base::FilePath GetDIPSFilePath(content::BrowserContext* context);

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
std::string_view SiteDataAccessTypeToString(SiteDataAccessType type);
std::ostream& operator<<(std::ostream& os, SiteDataAccessType access_type);

constexpr SiteDataAccessType operator|(SiteDataAccessType lhs,
                                       SiteDataAccessType rhs) {
  return static_cast<SiteDataAccessType>(static_cast<int>(lhs) |
                                         static_cast<int>(rhs));
}

// DIPSCookieMode:
enum class DIPSCookieMode { kBlock3PC, kOffTheRecord_Block3PC };

DIPSCookieMode GetDIPSCookieMode(bool is_otr);
std::string_view GetHistogramSuffix(DIPSCookieMode mode);
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

std::string_view GetHistogramPiece(DIPSRedirectType type);
const char* DIPSRedirectTypeToString(DIPSRedirectType type);
std::ostream& operator<<(std::ostream& os, DIPSRedirectType type);

using TimestampRange = std::optional<std::pair<base::Time, base::Time>>;
// Expand the range to include `time` if necessary. Returns true iff the range
// was modified.
bool UpdateTimestampRange(TimestampRange& range, base::Time time);
// Checks that `this` range is either null or falls within `other`.
bool IsNullOrWithin(const TimestampRange& inner, const TimestampRange& outer);

std::ostream& operator<<(std::ostream& os, TimestampRange type);

// Values for a site in the `bounces` table.
struct StateValue {
  TimestampRange site_storage_times;
  TimestampRange user_interaction_times;
  TimestampRange stateful_bounce_times;
  TimestampRange bounce_times;
  TimestampRange web_authn_assertion_times;
};

// Values for a 1P/3P site pair in the `popups` table.
struct PopupsStateValue {
  uint64_t access_id;
  base::Time last_popup_time;
  bool is_current_interaction;
  bool is_authentication_interaction;
};

struct PopupWithTime {
  std::string opener_site;
  std::string popup_site;
  base::Time last_popup_time;
};

enum class OptionalBool {
  kUnknown = 0,
  kFalse = 1,
  kTrue = 2,
};

inline OptionalBool ToOptionalBool(bool b) {
  return b ? OptionalBool::kTrue : OptionalBool::kFalse;
}

inline bool operator==(const StateValue& lhs, const StateValue& rhs) {
  return std::tie(lhs.site_storage_times, lhs.user_interaction_times,
                  lhs.stateful_bounce_times, lhs.bounce_times,
                  lhs.web_authn_assertion_times) ==
         std::tie(rhs.site_storage_times, rhs.user_interaction_times,
                  rhs.stateful_bounce_times, rhs.bounce_times,
                  rhs.web_authn_assertion_times);
}

// Return the number of seconds in `td`, clamped to [0, 10].
// i.e. 11 linearly-sized buckets.
int64_t BucketizeBounceDelay(base::TimeDelta delta);

// Returns an opaque value representing the "privacy boundary" that the URL
// belongs to. Currently returns eTLD+1, but this is an implementation detail
// and may change.
std::string GetSiteForDIPS(const GURL& url);
std::string GetSiteForDIPS(const url::Origin& origin);

// Returns true iff `web_contents` contains an iframe whose committed URL
// belongs to the same site as `url`.
bool HasSameSiteIframe(content::WebContents* web_contents, const GURL& url);

// Returns whether the provided cookie access was ad-tagged, based on the cookie
// settings overrides. Returns Unknown if kSkipTpcdMitigationsForAdsHeuristics
// is false and the override is not set regardless.
OptionalBool IsAdTaggedCookieForHeuristics(
    const content::CookieAccessDetails& details);

bool HasCHIPS(const net::CookieAccessResultList& cookie_access_result_list);

// Returns `True` iff the `navigation_handle` represents a navigation
// happening in an iframe of the primary frame tree.
inline bool IsInPrimaryPageIFrame(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle && navigation_handle->GetParentFrame()
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
  return navigation_handle && navigation_handle->GetParentFrame()
             ? navigation_handle->GetParentFrame()->GetPage().IsPrimary()
             : navigation_handle->IsInPrimaryMainFrame();
}

// Returns `True` iff the 'rfh' exists and represents a frame in the primary
// page.
inline bool IsInPrimaryPage(content::RenderFrameHost* rfh) {
  return rfh && rfh->GetPage().IsPrimary();
}

// Returns the last committed or the to be committed url of the main frame of
// the page containing the `navigation_handle`.
inline std::optional<GURL> GetFirstPartyURL(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return std::nullopt;
  }
  return navigation_handle->GetParentFrame()
             ? navigation_handle->GetParentFrame()
                   ->GetMainFrame()
                   ->GetLastCommittedURL()
             : navigation_handle->GetURL();
}

// Returns an optional last committed url of the main frame of the page
// containing the `rfh`.
inline std::optional<GURL> GetFirstPartyURL(content::RenderFrameHost* rfh) {
  return rfh ? std::optional<GURL>(rfh->GetMainFrame()->GetLastCommittedURL())
             : std::nullopt;
}

// The amount of time since a page last received user interaction before a
// subsequent user interaction event may be recorded to DIPS Storage for the
// same page.
extern const base::TimeDelta kDIPSTimestampUpdateInterval;

[[nodiscard]] bool UpdateTimestamp(std::optional<base::Time>& last_time,
                                   base::Time now);

enum class DIPSRecordedEvent {
  kStorage,
  kInteraction,
  kWebAuthnAssertion,
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
  kRead_EmptySite_InDb = 4,
  kRead_EmptySite_NotInDb = 5,
  kWrite_None = 6,
  kWrite_EmptySite = 7,
  kMaxValue = kWrite_EmptySite,
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
  kExceptedAs1p = 1,  // No longer used - merged into 'kExcepted' below.
  kExceptedAs3p = 2,  // No longer used - merged into 'kExcepted' below.
  kEnforced = 3,
  kIgnored = 4,
  kExcepted = 5,
  kMaxValue = kExcepted,
};

enum class DIPSDatabaseTable {
  kBounces = 1,
  kPopups = 2,
  kMaxValue = kPopups,
};

#endif  // CHROME_BROWSER_DIPS_DIPS_UTILS_H_
