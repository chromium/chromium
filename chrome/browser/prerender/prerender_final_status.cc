// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_final_status.h"

#include "base/stl_util.h"
#include "chrome/browser/prerender/prerender_manager.h"

namespace prerender {

namespace {

const char* kFinalStatusNames[] = {
    "Used",
    "Timed Out",
    "Evicted",
    "Manager Shutdown",
    "Closed",
    "Create New Window",
    "Profile Destroyed",
    "App Terminating",
    "Javascript Alert",
    "Auth Needed",
    "HTTPS",
    "Download",
    "Memory Limit Exceeded",
    "JS Out Of Memory",
    "Renderer Unresponsive",
    "Too many processes",
    "Rate Limit Exceeded",
    "Pending Skipped",
    "Control Group",
    "HTML5 Media",
    "Source Render View Closed",
    "Renderer Crashed",
    "Unsupported Scheme",
    "Invalid HTTP Method",
    "window.print",
    "Recently Visited",
    "window.opener",
    "Page ID Conflict",
    "Safe Browsing",
    "Fragment Mismatch",
    "SSL Client Certificate Requested",
    "Cache or History Cleared",
    "Cancelled",
    "SSL Error",
    "Cross-Site Navigation Pending",
    "DevTools Attached To The Tab",
    "Session Storage Namespace Mismatch",
    "No Use Group",
    "Match Complete Dummy",
    "Duplicate",
    "OpenURL",
    "WouldHaveBeenUsed",
    "Register Protocol Handler",
    "Creating Audio Stream",
    "Page Being Captured",
    "Bad Deferred Redirect",
    "Navigation Uncommitted",
    "New Navigation Entry",
    "Cookie Store Not Loaded",
    "Cookie Conflict",
    "Non-Empty Browsing Instance",
    "Navigation Intercepted",
    "Prerendering Disabled",
    "Cellular Network",
    "Block Third Party Cookies",
    "Credential Manager API",
    "NoStatePrefetch Finished",
    "Low-End Device",
    "BrowserSwitcher Switch",
    "GWS Holdback",
    "Unknown",
    "Navigation Predictor Holdback",
    "Max",
};
static_assert(base::size(kFinalStatusNames) == FINAL_STATUS_MAX + 1,
              "status name count mismatch");

}  // namespace

const char* NameFromFinalStatus(FinalStatus final_status) {
  DCHECK_LT(static_cast<unsigned int>(final_status),
            base::size(kFinalStatusNames));
  return kFinalStatusNames[final_status];
}

}  // namespace prerender
