// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#endif

namespace apps::test {

std::string ToString(LinkCapturingFeatureVersion version) {
  switch (version) {
    case LinkCapturingFeatureVersion::kV1DefaultOff:
      return "V1DefaultOff";
    case LinkCapturingFeatureVersion::kV2DefaultOff:
      return "V2DefaultOff";
#if !BUILDFLAG(IS_CHROMEOS)
    case LinkCapturingFeatureVersion::kV1DefaultOn:
      return "V1DefaultOn";
    case LinkCapturingFeatureVersion::kV2DefaultOn:
      return "V2DefaultOn";
#endif
  }
}

std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    std::optional<bool> override_captures_by_default,
    bool use_v2) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  CHECK(!override_captures_by_default || !override_captures_by_default.value());
  // TODO(crbug.com/376922620): Create a feature flag to turn off the v1
  // throttle.
  if (use_v2) {
    return {{::features::kPwaNavigationCapturing,
             {{::features::kNavigationCapturingDefaultState.name,
               "reimpl_default_off"}}}};
  }
  return {{::apps::features::kLinkCapturingUiUpdate, {}}};
#else
  // TODO(crbug.com/351775835): Integrate testing for all enum states of
  // `CapturingState`.
  std::string on_by_default_label =
      use_v2 ? "reimpl_default_on" : "on_by_default";
  std::string off_by_default_label =
      use_v2 ? "reimpl_default_off" : "off_by_default";

  bool should_override_by_default = override_captures_by_default.value_or(
      ::features::kNavigationCapturingDefaultState.default_value ==
          ::features::CapturingState::kDefaultOn ||
      ::features::kNavigationCapturingDefaultState.default_value ==
          ::features::CapturingState::kReimplDefaultOn);

  return {{::features::kPwaNavigationCapturing,
           {{::features::kNavigationCapturingDefaultState.name,
             std::string(should_override_by_default ? on_by_default_label
                                                    : off_by_default_label)}}}};
#endif  // BUILDFLAG(IS_CHROMEOS)
}

std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    LinkCapturingFeatureVersion version) {
  switch (version) {
    case LinkCapturingFeatureVersion::kV1DefaultOff:
      return GetFeaturesToEnableLinkCapturingUX(
          /*override_captures_by_default=*/false, /*use_v2=*/false);
    case LinkCapturingFeatureVersion::kV2DefaultOff:
      return GetFeaturesToEnableLinkCapturingUX(
          /*override_captures_by_default=*/false, /*use_v2=*/true);
#if !BUILDFLAG(IS_CHROMEOS)
    case LinkCapturingFeatureVersion::kV1DefaultOn:
      return GetFeaturesToEnableLinkCapturingUX(
          /*override_captures_by_default=*/true, /*use_v2=*/false);
    case LinkCapturingFeatureVersion::kV2DefaultOn:
      return GetFeaturesToEnableLinkCapturingUX(
          /*override_captures_by_default=*/true, /*use_v2=*/true);
#endif
  }
}

std::vector<base::test::FeatureRef> GetFeaturesToDisableLinkCapturingUX() {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  return {::apps::features::kLinkCapturingUiUpdate};
#else
  return {::features::kPwaNavigationCapturing};
#endif  // BUILDFLAG(IS_CHROMEOS)
}

base::expected<void, std::string> EnableLinkCapturingByUser(
    Profile* profile,
    const webapps::AppId& app_id) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  apps_util::SetSupportedLinksPreferenceAndWait(profile, app_id);
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(profile);
  base::test::TestFuture<void> preference_set;
  provider->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, /*set_to_preferred=*/true, preference_set.GetCallback());
  if (!preference_set.Wait()) {
    return base::unexpected("Enable link capturing command never completed.");
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return base::ok();
}

base::expected<void, std::string> DisableLinkCapturingByUser(
    Profile* profile,
    const webapps::AppId& app_id) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  apps_util::RemoveSupportedLinksPreferenceAndWait(profile, app_id);
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(profile);
  base::test::TestFuture<void> preference_set;
  provider->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, /*set_to_preferred=*/false, preference_set.GetCallback());
  if (!preference_set.Wait()) {
    return base::unexpected("Disable link capturing command never completed.");
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return base::ok();
}

NavigationCommittedForUrlObserver::NavigationCommittedForUrlObserver(
    const GURL& url)
    : url_(url) {
  AddAllBrowsers();
}

NavigationCommittedForUrlObserver::~NavigationCommittedForUrlObserver() =
    default;

std::unique_ptr<base::CheckedObserver>
NavigationCommittedForUrlObserver::ProcessOneContents(
    content::WebContents* web_contents) {
  return std::make_unique<content::DidFinishNavigationObserver>(
      web_contents, base::BindRepeating(
                        &NavigationCommittedForUrlObserver::DidFinishNavigation,
                        base::Unretained(this)));
}

void NavigationCommittedForUrlObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->HasCommitted()) {
    return;
  }
  if (!handle->IsInMainFrame() || handle->GetURL() != url_) {
    return;
  }
  // Record the first match.
  if (!web_contents_) {
    web_contents_ = handle->GetWebContents();
  }
  ConditionMet();
}

}  // namespace apps::test
