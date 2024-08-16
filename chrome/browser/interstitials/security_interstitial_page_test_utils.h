// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_TEST_UTILS_H_
#define CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_TEST_UTILS_H_

#include <string>

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace chrome_browser_interstitials {

enum class HFMInterstitialType {
  // No HFM interstitial is being displayed.
  kNone,
  // The standard, non-heuristic and non-incognito interstitial is being
  // displayed.
  kStandard,
  // The incognito interstitial is being displayed.
  kIncognito,
  // The interstitial is being displayed due to the Typically Secure User
  // heuristic.
  kTypicallySecure,
  // The interstitial is being displayed due to the Site Engagement heuristic.
  kSiteEngagement,
};

// Looks for text in the |textContent| of |interstitial_frame|'s body and
// returns true if found. This can be used for either transient or committed
// interstitials. For the former, pass
// web_contents->GetInterstitialPage()->GetPrimaryMainFrame() as the first
// argument, and for the latter, just pass web_contents->GetPrimaryMainFrame().
bool IsInterstitialDisplayingText(content::RenderFrameHost* interstitial_frame,
                                  const std::string& text);

// Returns true if |interstitial_frame| allows the user to override the
// displayed interstitial.
bool InterstitialHasProceedLink(content::RenderFrameHost* interstitial_frame);

// Returns true if |tab| is currently displaying an interstitial.
bool IsShowingInterstitial(content::WebContents* tab);

// The functions below might start causing tests to fail if you change the
// strings that appear on interstitials. If that happens, it's fine to update
// the keywords that are checked for in each interstitial. But the keywords
// should remain fairly unique for each interstitial to ensure that the tests
// check that the proper interstitial comes up. For example, it wouldn't be good
// to simply look for the word "security" because that likely shows up on lots
// of different types of interstitials, not just the type being tested for.

// Returns true if |tab| is displaying a captive-portal related interstitial.
bool IsShowingCaptivePortalInterstitial(content::WebContents* tab);

// Returns true if |tab| is currently displaying an SSL-related interstitial.
bool IsShowingSSLInterstitial(content::WebContents* tab);

// Returns true if |tab| is displaying a MITM-related interstitial.
bool IsShowingMITMInterstitial(content::WebContents* tab);

// Returns true if |tab| is displaying a clock-related interstitial.
bool IsShowingBadClockInterstitial(content::WebContents* tab);

// Returns true if |tab| is displaying a known-interception interstitial.
bool IsShowingBlockedInterceptionInterstitial(content::WebContents* tab);

// Returns true if `tab` is displaying any variant of the HTTPS-First Mode
// interstitial, including those that can be shown due to HFM heuristics.
bool IsShowingHttpsFirstModeInterstitial(content::WebContents* tab);

// Returns the type of the HTTPS-First Mode interstitial being displayed in
// `tab`. If no HFM interstitial is being displayed, returns kNone.
HFMInterstitialType GetHFMInterstitialType(content::WebContents* tab);

}  // namespace chrome_browser_interstitials

#endif  // CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_TEST_UTILS_H_
