// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#endif

namespace apps::test {

std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    std::optional<bool> override_captures_by_default) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  CHECK(!override_captures_by_default || !override_captures_by_default.value());
  return {{::apps::features::kLinkCapturingUiUpdate, {}}};
#else
  // TODO(crbug.com/351775835): Integrate testing for all enum states of
  // `CapturingState`.
  bool should_override_by_default = override_captures_by_default.value_or(
      ::features::kNavigationCapturingDefaultState.default_value ==
      ::features::CapturingState::kDefaultOn);
  return {{::features::kPwaNavigationCapturing,
           {{::features::kNavigationCapturingDefaultState.name,
             std::string(should_override_by_default ? "on_by_default"
                                                    : "off_by_default")}}}};
#endif  // BUILDFLAG(IS_CHROMEOS)
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

}  // namespace apps::test
