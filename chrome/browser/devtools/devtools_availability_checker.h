// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_

class GURL;
class Profile;

namespace content {
class DevToolsAgentHost;
class RenderFrameHost;
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

// |rfh| may be null, in which case this function just checks the settings for
// |profile|. When target-level evaluation is enabled, this checks the frame's
// own last committed URL before falling back to its owning WebContents.
DevToolsBlockReason GetDevToolsBlockReason(Profile* profile,
                                           content::RenderFrameHost* rfh);

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
bool IsInspectionAllowed(Profile* profile, content::RenderFrameHost* rfh);
bool IsInspectionAllowed(Profile* profile,
                         const extensions::Extension* extension);
bool IsInspectionAllowed(Profile* profile, const web_app::WebApp* web_app);
bool IsInspectionAllowed(Profile* profile, const GURL& url);
bool IsInspectionAllowed(Profile* profile,
                         content::DevToolsAgentHost* agent_host);

// Returns true if the DevTools UI URL (`devtools://devtools/...`) is blocked on
// `profile` by the generic `URLBlocklist` policy. `profile` must not be null.
bool IsDevToolsUrlBlockedByPolicy(Profile* profile);

// Returns true if at least one policy check that can affect DevTools
// availability applies to `profile`:
// - `DeveloperToolsAvailability` (or legacy `DeveloperToolsDisabled`) is set by
//   policy,
// - `DeveloperToolsAvailabilityAllowlist` or
//   `DeveloperToolsAvailabilityBlocklist` is non-empty,
// - the DevTools UI URL is blocked by `URLBlocklist`
//   (`IsDevToolsUrlBlockedByPolicy()`), or
// - effective `DeveloperToolsAvailability` has its default value
//   (`DisallowedForForceInstalledExtensions`) and either `profile` is managed
//   (when extensions are enabled, since built-in component extensions are
//   restricted in managed profiles) or `profile` has an installed extension or
//   web app restricted under that default (a policy-installed extension, a
//   kiosk web app, or a policy-installed Isolated Web App).
// `profile` must not be null. Used to slice DevTools UMA metrics to profiles
// where policy checks can affect DevTools availability.
bool HasDevToolsPolicyCheck(Profile* profile);

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AVAILABILITY_CHECKER_H_
