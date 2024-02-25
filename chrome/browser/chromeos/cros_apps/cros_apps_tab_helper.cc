// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/cros_apps_tab_helper.h"

#include <memory>

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_frame_context.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_registry.h"
#include "chrome/browser/profiles/profile.h"
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
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());

  auto enable_fns = CrosAppsApiRegistry::GetInstance(profile)
                        .GetBlinkFeatureEnablementFunctionsForFrame(
                            CrosAppsApiFrameContext(*navigation_handle));

  if (enable_fns.empty()) {
    return;
  }

  auto& runtime_feature_state_context =
      navigation_handle->GetMutableRuntimeFeatureStateContext();
  runtime_feature_state_context.SetBlinkExtensionChromeOSEnabled(true);
  for (const auto& enable_fn : enable_fns) {
    (runtime_feature_state_context.*enable_fn)(true);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CrosAppsTabHelper);
