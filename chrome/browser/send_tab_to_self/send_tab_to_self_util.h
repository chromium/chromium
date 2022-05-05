// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {

enum class EntryPointDisplayReason {
  // The send-tab-to-self entry point should be shown because all the conditions
  // are met and the feature is ready to be used.
  kOfferFeature,
  // The user might be able to use send-tab-to-self if they sign in, so offer
  // that. "Might" because the list of target devices can't be known yet, it
  // could be empty (see below).
  kOfferSignIn,
  // All the conditions for send-tab-to-self are met, but there is no valid
  // target device. In that case the entry point should inform the user they
  // can enjoy the feature by signing in on other devices.
  kInformNoTargetDevice,
};

absl::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    content::WebContents* web_contents);

// Returns true if the entry point should be shown.
bool ShouldDisplayEntryPoint(content::WebContents* web_contents);

// Returns true if the omnibox icon for the feature should be offered.
bool ShouldOfferOmniboxIcon(content::WebContents* web_contents);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
