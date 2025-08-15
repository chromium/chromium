// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_tab_helper.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
AskBeforeHttpDialogController* GetAskBeforeHttpDialogController(
    content::WebContents* web_contents) {
  // WebContentsObserver events can fire for things that aren't a tab and don't
  // have a TabInterface associated.
  // TODO(crbug.com/351990829): Filter these cases out before this gets called
  // and then change this to call TabInterface::GetFromContents() instead.
  auto* const tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }
  AskBeforeHttpDialogController* dialog_controller =
      tab->GetTabFeatures()->ask_before_http_dialog_controller();
  return dialog_controller;
}
#endif

}  // namespace

HttpsOnlyModeTabHelper::~HttpsOnlyModeTabHelper() = default;

void HttpsOnlyModeTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsFirstDialogUi)) {
    // Close the Ask-before-HTTP dialog if a new navigation begins.
    // TabDialogManager has a parameter to close tab-modal dialogs
    // if a cross-site navigation occurs, but we need to be more
    // strict and close on *any* new navigation. (For example, without
    // this when a user clicks from badssl.com to http.badssl.com, sees
    // the warning prompt, and then clicks the back button, the warning
    // prompt would still be showing.)
    auto* const dialog_controller =
        GetAskBeforeHttpDialogController(navigation_handle->GetWebContents());
    if (dialog_controller && dialog_controller->HasOpenDialogWidget()) {
      // TODO(crbug.com/351990829): Consider adding a new `ClosedReason`
      // value for this case.
      dialog_controller->CloseDialogWidget(
          views::Widget::ClosedReason::kUnspecified);
    }
  }
#endif

  // If the user was on an exempt net error and the tab was reloaded, only
  // reset the exempt error state, but keep the upgrade state so the reload
  // will result in continuing to attempt the upgraded navigation (and if it
  // later fails, the fallback will be to the original fallback URL).
  bool should_maintain_upgrade_state =
      is_exempt_error() &&
      navigation_handle->GetReloadType() != content::ReloadType::NONE;
  set_is_exempt_error(false);
  if (!should_maintain_upgrade_state) {
    set_fallback_url(GURL());
    set_is_navigation_fallback(false);
    set_is_navigation_upgraded(false);
  }
}

void HttpsOnlyModeTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/351990829): Consider if this check could be made more
  // precise. One option mght be to communicate a "interstitial enabled" flag on
  // the helper which is set by the throttle.
  // If dialog UI is enabled and this was a navigation cancelled by the
  // throttle when we would interstitial the navigation, then trigger the
  // dialog UI.
  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsFirstDialogUi) &&
      is_navigation_fallback_ &&
      !navigation_handle->GetURL().SchemeIsCryptographic()) {
    auto* const dialog_controller =
        GetAskBeforeHttpDialogController(navigation_handle->GetWebContents());
    if (!dialog_controller) {
      // If there is no dialog controller, then the tab is being destroyed
      // and there is nothing to show a modal on. Just return.
      return;
    }
    ukm::SourceId ukm_source_id = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
    dialog_controller->ShowDialog(navigation_handle->GetWebContents(),
                                  navigation_handle->GetURL(), ukm_source_id);
    // Make sure that the security indicator shows the correct icon.
    // TODO(crbug.com/351990829): Add the new icon and integration.
    navigation_handle->GetWebContents()->DidChangeVisibleSecurityState();
  }
#endif
}

HttpsOnlyModeTabHelper::HttpsOnlyModeTabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<HttpsOnlyModeTabHelper>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HttpsOnlyModeTabHelper);
