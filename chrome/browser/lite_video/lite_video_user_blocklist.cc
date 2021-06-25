// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_user_blocklist.h"

#include "chrome/browser/lite_video/lite_video_features.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Determine whether the URL is valid and can be queried or
// added to the blocklist.
bool IsURLValidForBlocklist(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() && url.has_host();
}

// Separator between hosts for the rebuffer blocklist type.
constexpr char kLiteVideoBlocklistKeySeparator[] = "_";

// Returns the key for a navigation used for the rebuffer blocklist type.
// The key format is "mainframe.com_subframe.com", if the navigation is the
// mainframe navigation, the key omits subframe.com, e.g., "mainframe.com_"
absl::optional<std::string> GetRebufferBlocklistKey(
    const GURL& mainframe_url,
    absl::optional<GURL> subframe_url) {
  if (!IsURLValidForBlocklist(mainframe_url))
    return absl::nullopt;

  if (!subframe_url)
    return mainframe_url.host() + kLiteVideoBlocklistKeySeparator;

  if (!IsURLValidForBlocklist(*subframe_url))
    return absl::nullopt;
  return mainframe_url.host() + kLiteVideoBlocklistKeySeparator +
         subframe_url->host();
}

}  // namespace

namespace lite_video {

LiteVideoUserBlocklist::LiteVideoUserBlocklist(
    std::unique_ptr<blocklist::OptOutStore> opt_out_store,
    base::Clock* clock,
    blocklist::OptOutBlocklistDelegate* blocklist_delegate)
    : OptOutBlocklist(std::move(opt_out_store), clock, blocklist_delegate) {
  Init();
}

LiteVideoUserBlocklist::~LiteVideoUserBlocklist() = default;

LiteVideoBlocklistReason LiteVideoUserBlocklist::IsLiteVideoAllowedOnNavigation(
    content::NavigationHandle* navigation_handle) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GURL navigation_url = navigation_handle->GetURL();
  if (!IsURLValidForBlocklist(navigation_url))
    return LiteVideoBlocklistReason::kNavigationNotEligibile;

  std::vector<blocklist::BlocklistReason> passed_reasons;
  auto blocklist_reason = blocklist::OptOutBlocklist::IsLoadedAndAllowed(
      navigation_url.host(),
      static_cast<int>(LiteVideoBlocklistType::kNavigationBlocklist),
      /*opt_out=*/false, &passed_reasons);
  if (blocklist_reason != blocklist::BlocklistReason::kAllowed)
    return LiteVideoBlocklistReason::kNavigationBlocklisted;

  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  absl::optional<std::string> rebuffer_key =
      navigation_handle->IsInPrimaryMainFrame()
          ? GetRebufferBlocklistKey(navigation_url, absl::nullopt)
          : GetRebufferBlocklistKey(
                navigation_handle->GetWebContents()->GetLastCommittedURL(),
                navigation_url);

  if (!rebuffer_key)
    return LiteVideoBlocklistReason::kNavigationNotEligibile;

  blocklist_reason = blocklist::OptOutBlocklist::IsLoadedAndAllowed(
      *rebuffer_key,
      static_cast<int>(LiteVideoBlocklistType::kRebufferBlocklist),
      /*opt_out=*/false, &passed_reasons);
  if (blocklist_reason != blocklist::BlocklistReason::kAllowed)
    return LiteVideoBlocklistReason::kRebufferingBlocklisted;
  return LiteVideoBlocklistReason::kAllowed;
}

bool LiteVideoUserBlocklist::ShouldUseSessionPolicy(base::TimeDelta* duration,
                                                    size_t* history,
                                                    int* threshold) const {
  return false;
}

bool LiteVideoUserBlocklist::ShouldUsePersistentPolicy(
    base::TimeDelta* duration,
    size_t* history,
    int* threshold) const {
  return false;
}

bool LiteVideoUserBlocklist::ShouldUseHostPolicy(base::TimeDelta* duration,
                                                 size_t* history,
                                                 int* threshold,
                                                 size_t* max_hosts) const {
  DCHECK(duration);
  DCHECK(history);
  DCHECK(threshold);
  DCHECK(max_hosts);
  *max_hosts = features::MaxUserBlocklistHosts();
  *duration = features::UserBlocklistHostDuration();
  *threshold = features::UserBlocklistOptOutHistoryThreshold();
  *history = features::UserBlocklistOptOutHistoryThreshold();
  return true;
}

bool LiteVideoUserBlocklist::ShouldUseTypePolicy(base::TimeDelta* duration,
                                                 size_t* history,
                                                 int* threshold) const {
  return false;
}

blocklist::BlocklistData::AllowedTypesAndVersions
LiteVideoUserBlocklist::GetAllowedTypes() const {
  return {{static_cast<int>(LiteVideoBlocklistType::kNavigationBlocklist),
           features::LiteVideoBlocklistVersion()},
          {static_cast<int>(LiteVideoBlocklistType::kRebufferBlocklist),
           features::LiteVideoBlocklistVersion()}};
}

void LiteVideoUserBlocklist::AddNavigationToBlocklist(
    content::NavigationHandle* navigation_handle,
    bool opt_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsURLValidForBlocklist(navigation_handle->GetURL()))
    return;
  AddEntry(navigation_handle->GetURL().host(), opt_out,
           static_cast<int>(LiteVideoBlocklistType::kNavigationBlocklist));
}

void LiteVideoUserBlocklist::AddRebufferToBlocklist(
    const GURL& mainframe_url,
    absl::optional<GURL> subframe_url,
    bool opt_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::optional<std::string> rebuffer_key =
      GetRebufferBlocklistKey(mainframe_url, subframe_url);
  if (rebuffer_key) {
    AddEntry(*rebuffer_key, opt_out,
             static_cast<int>(LiteVideoBlocklistType::kRebufferBlocklist));
  }
}

}  // namespace lite_video
