// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_USER_BLOCKLIST_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_USER_BLOCKLIST_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "url/gurl.h"

namespace base {
class Clock;
}  // namespace base

namespace content {
class NavigationHandle;
}  // namespace content

namespace lite_video {

// The current LiteVideo blocklists types.
enum class LiteVideoBlocklistType {
  kNone,
  // Blocklist for hosts with too many media rebuffer events.
  // Keyed by a navigation's host combined with the mainframe host.
  kRebufferBlocklist,
  // Blocklist for hosts with too many reloads or back-forward navigations.
  // Keyed by the mainframe host.
  kNavigationBlocklist,

  // Insert new values before this line.
  kMaxValue = kNavigationBlocklist,
};

// The reasons a navigation could be blocklisted by the LiteVideoUserBlocklist.
// This should be kept in sync with LiteVideoBlocklistReason in enums.xml.
enum class LiteVideoBlocklistReason {
  kUnknown,
  // The navigation is allowed by all types of this LiteVideoUserBlocklist.
  kAllowed,
  // The LiteVideo optimization does not support this type navigation (e.g,. not
  // HTTP/HTTPS or no host).
  kNavigationNotEligibile,
  // LiteVideos were blocked because the host was on the
  // RebufferBlocklist.
  kRebufferingBlocklisted,
  // LiteVideos were blocked because the host was on the
  // NavigationBlocklist.
  kNavigationBlocklisted,
  // LiteVideo not shown because it was a reloaded navigation.
  kNavigationReload,
  // LiteVideo not shown because it was a forward-back navigation.
  kNavigationForwardBack,
  // LiteVideo not shown because the host was permanently blocklisted.
  kHostPermanentlyBlocklisted,

  // Insert new values before this line.
  kMaxValue = kHostPermanentlyBlocklisted,
};

// The LiteVideoUserBlocklist maintains information about hosts the user
// navigates to and are perceived to be low-quaility experiences because of
// throttling media requests. If the user frequently has a low-quality
// experience on a particular host, it will be added to the blocklist, disabling
// LiteVideos for that host. Currently, hosts are added to the blocklist based
// on excess rebuffers and frequent reloads/back-forward navigations.
class LiteVideoUserBlocklist : public blocklist::OptOutBlocklist {
 public:
  LiteVideoUserBlocklist(
      std::unique_ptr<blocklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      blocklist::OptOutBlocklistDelegate* blocklist_delegate);
  ~LiteVideoUserBlocklist() override;

  // Determine if the navigation is blocklisted by checking the current
  // blocklists. Returns if it is blocklisted or LiteVideos should be allowed.
  // Virtual for testing.
  virtual LiteVideoBlocklistReason IsLiteVideoAllowedOnNavigation(
      content::NavigationHandle* navigation_handle) const;

  // Update the entry within the NavigationBlocklistType for the
  // |navigation_handle| based on whether it was an opt-out or not.
  void AddNavigationToBlocklist(content::NavigationHandle* navigation_handle,
                                bool opt_out);

  // Update the entry within the RebufferBlocklistType for the
  // mainframe and subframe urls based on whether it was an opt-out or not.
  void AddRebufferToBlocklist(const GURL& mainframe_url,
                              base::Optional<GURL> subframe_url,
                              bool opt_out);

 protected:
  // OptOutBlocklist:
  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override;
  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override;
  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override;
  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override;
  blocklist::BlocklistData::AllowedTypesAndVersions GetAllowedTypes()
      const override;

 private:

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_USER_BLOCKLIST_H_
