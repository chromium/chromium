// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/cros_apps_tab_helper.h"

#include <memory>

#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"

void CrosAppsTabHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  // TODO(b/295267987): Use a CrOS Apps flag when available.
  if (!chromeos::features::IsBlinkExtensionEnabled()) {
    return;
  }

  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(), std::make_unique<CrosAppsTabHelper>(
                           web_contents, base::PassKey<CrosAppsTabHelper>()));
  }
}

CrosAppsTabHelper::CrosAppsTabHelper(content::WebContents* web_contents,
                                     base::PassKey<CrosAppsTabHelper>)
    : content::WebContentsUserData<CrosAppsTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents) {}

CrosAppsTabHelper::~CrosAppsTabHelper() = default;

void CrosAppsTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // TODO(b/295267987): Check `navigation_handle` is for a ChromeOS Apps and
  // set features accordingly.

  if (chromeos::features::IsBlinkExtensionEnabled()) {
    navigation_handle->GetMutableRuntimeFeatureStateContext()
        .SetBlinkExtensionChromeOSEnabled(true);
  }

  if (chromeos::features::IsBlinkExtensionDiagnosticsEnabled()) {
    navigation_handle->GetMutableRuntimeFeatureStateContext()
        .SetBlinkExtensionDiagnosticsEnabled(true);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CrosAppsTabHelper);
