// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace chrome_browser_interstitials {

bool IsInterstitialDisplayingText(content::RenderFrameHost* interstitial_frame,
                                  const std::string& text) {
  // It's valid for |text| to contain "\'", but simply look for "'" instead
  // since this function is used for searching host names and a predefined
  // string.
  DCHECK(text.find("\'") == std::string::npos);
  std::string command = base::StringPrintf(
      "var hasText = document.body.textContent.indexOf('%s') >= 0;"
      "hasText ? %d : %d;",
      text.c_str(), security_interstitials::CMD_TEXT_FOUND,
      security_interstitials::CMD_TEXT_NOT_FOUND);
  return content::EvalJs(interstitial_frame, command).ExtractInt() ==
         security_interstitials::CMD_TEXT_FOUND;
}

bool InterstitialHasProceedLink(content::RenderFrameHost* interstitial_frame) {
  const std::string javascript = base::StringPrintf(
      "document.querySelector(\"#proceed-link\") === null "
      "? %d : %d",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  return content::EvalJs(interstitial_frame, javascript).ExtractInt() ==
         security_interstitials::CMD_TEXT_FOUND;
}

bool IsShowingInterstitial(content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  return helper && helper->IsDisplayingInterstitial();
}

bool IsShowingCaptivePortalInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(), "Connect to");
}

bool IsShowingSSLInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(),
                                      "Your connection is not private");
}

bool IsShowingMITMInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(),
                                      "An application is stopping");
}

bool IsShowingBadClockInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(),
                                      "Your clock is");
}

bool IsShowingBlockedInterceptionInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(),
                                      "Anything you type, any pages you view");
}

bool IsShowingHttpsFirstModeInterstitial(content::WebContents* tab) {
  return GetHFMInterstitialType(tab) != HFMInterstitialType::kNone;
}

HFMInterstitialType GetHFMInterstitialType(content::WebContents* tab) {
  if (!IsShowingInterstitial(tab)) {
    // Check if it's a dialog.
    auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(tab);
    if (tab_interface) {
      auto* controller = AskBeforeHttpDialogController::From(tab_interface);
      if (controller && controller->HasOpenDialog()) {
        auto reason = controller->GetInterstitialReasonForTesting();
        switch (reason) {
          case security_interstitials::https_only_mode::InterstitialReason::
              kSiteEngagementHeuristic:
            return HFMInterstitialType::kSiteEngagement;
          case security_interstitials::https_only_mode::InterstitialReason::
              kTypicallySecureUserHeuristic:
            return HFMInterstitialType::kTypicallySecure;
          case security_interstitials::https_only_mode::InterstitialReason::
              kIncognito:
            return HFMInterstitialType::kIncognito;
          case security_interstitials::https_only_mode::InterstitialReason::
              kPref:
          case security_interstitials::https_only_mode::InterstitialReason::
              kBalanced:
            return HFMInterstitialType::kStandard;
          default:
            return HFMInterstitialType::kStandard;
        }
      }
    }
    return HFMInterstitialType::kNone;
  }
  if (IsInterstitialDisplayingText(
          tab->GetPrimaryMainFrame(),
          "You usually connect to this site securely")) {
    return HFMInterstitialType::kSiteEngagement;
  }
  if (IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(),
                                   "You usually connect to sites securely")) {
    return HFMInterstitialType::kTypicallySecure;
  }
  if (IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(),
                                   "this site does not support HTTPS and "
                                   "you are in Incognito mode")) {
    return HFMInterstitialType::kIncognito;
  }
  if (IsInterstitialDisplayingText(tab->GetPrimaryMainFrame(),
                                   "doesn’t support a secure connection")) {
    return HFMInterstitialType::kStandard;
  }
  return HFMInterstitialType::kNone;
}

void ProceedThroughHttpsFirstModeInterstitial(content::WebContents* tab) {
  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(tab);
  if (tab_interface) {
    auto* controller = AskBeforeHttpDialogController::From(tab_interface);
    if (controller && controller->HasOpenDialog()) {
      content::TestNavigationObserver nav_observer(tab, 1);
      controller->ProceedForTesting();
      nav_observer.Wait();
      return;
    }
  }
  content::TestNavigationObserver nav_observer(tab, 1);
  std::string javascript = "window.certificateErrorPageController.proceed();";
  ASSERT_TRUE(content::ExecJs(tab, javascript));
  nav_observer.Wait();
}

void DontProceedThroughHttpsFirstModeInterstitial(content::WebContents* tab) {
  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(tab);
  if (tab_interface) {
    auto* controller = AskBeforeHttpDialogController::From(tab_interface);
    if (controller && controller->HasOpenDialog()) {
      content::TestNavigationObserver nav_observer(tab, 1);
      controller->CancelForTesting();
      nav_observer.Wait();
      return;
    }
  }
  content::TestNavigationObserver nav_observer(tab, 1);
  std::string javascript =
      "window.certificateErrorPageController.dontProceed();";
  ASSERT_TRUE(content::ExecJs(tab, javascript));
  nav_observer.Wait();
}

}  // namespace chrome_browser_interstitials
