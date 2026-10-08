// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_

class GURL;
class Profile;

namespace content {
class DevToolsAgentHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

namespace web_app {
class WebApp;
}

// Why DevTools are not available for a given context. Recorded in UMA by
// DevToolsPolicyDialog when the user is told that DevTools are blocked.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DevToolsBlockReason)
enum class DevToolsBlockReason {
  // DevTools are available.
  kNotBlocked = 0,
  // The URL is blocked by the DeveloperToolsAvailabilityBlocklist policy, or
  // is not covered by a non-empty DeveloperToolsAvailabilityAllowlist policy.
  kUrlAllowlistOrBlocklist = 1,
  // The DeveloperToolsAvailability policy is set to Disallowed.
  kPolicyDisallowed = 2,
  // The context belongs to a policy-installed extension and the
  // DeveloperToolsAvailability policy has its default value
  // (DisallowedForForceInstalledExtensions).
  kForceInstalledExtension = 3,
  // The context belongs to a component extension (e.g. the PDF viewer) in a
  // managed profile and the DeveloperToolsAvailability policy has its default
  // value.
  kComponentExtensionInManagedProfile = 4,
  // The context belongs to a kiosk-installed web app and the
  // DeveloperToolsAvailability policy has its default value.
  kKioskWebApp = 5,
  // The context belongs to a policy-installed Isolated Web App and the
  // DeveloperToolsAvailability policy has its default value.
  kPolicyInstalledIsolatedWebApp = 6,
  // The profile is a ChromeOS captive portal sign-in window.
  kCaptivePortalSignin = 7,
  // The DevTools UI is unavailable because Chrome runs in kiosk mode. Only
  // reported by DevToolsWindow::GetBlockReasonFor().
  kKioskMode = 8,
  // The DevTools UI is unavailable because chrome://devtools is blocked by
  // the URLBlocklist policy. Only reported by
  // DevToolsWindow::GetBlockReasonFor().
  kDevToolsUrlBlocklisted = 9,
  kMaxValue = kDevToolsUrlBlocklisted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/dev/enums.xml:DevToolsBlockReason)

// The GetDevToolsBlockReason() overloads below return why the DevTools
// policies block inspection of the given context, or
// DevToolsBlockReason::kNotBlocked if inspection is allowed.

// |web_contents| may be null, in which case this function just checks
// the settings for |profile|.
DevToolsBlockReason GetDevToolsBlockReason(Profile* profile,
                                           content::WebContents* web_contents);

// |extension| may be null, in which case this function just checks
// the settings for |profile|.
DevToolsBlockReason GetDevToolsBlockReason(
    Profile* profile,
    const extensions::Extension* extension);

// |web_app| may be null, in which case this function just checks
// the settings for |profile|.
DevToolsBlockReason GetDevToolsBlockReason(Profile* profile,
                                           const web_app::WebApp* web_app);

// Checks the developer tools policies for a given |url|.
DevToolsBlockReason GetDevToolsBlockReason(Profile* profile, const GURL& url);

DevToolsBlockReason GetDevToolsBlockReason(
    Profile* profile,
    content::DevToolsAgentHost* agent_host);

// Boolean equivalents of the GetDevToolsBlockReason() overloads above: each
// returns true iff the corresponding overload returns
// DevToolsBlockReason::kNotBlocked. The notes on null arguments above apply
// here as well.
bool IsInspectionAllowed(Profile* profile, content::WebContents* web_contents);
bool IsInspectionAllowed(Profile* profile,
                         const extensions::Extension* extension);
bool IsInspectionAllowed(Profile* profile, const web_app::WebApp* web_app);
bool IsInspectionAllowed(Profile* profile, const GURL& url);
bool IsInspectionAllowed(Profile* profile,
                         content::DevToolsAgentHost* agent_host);

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_
