// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

namespace glic {

class GlicPermissionEnforcementBrowserTest
    : public GlicApiBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  GlicPermissionEnforcementBrowserTest()
      : GlicApiBrowserTest(
            GlicTestJsPath("./glic_permission_enforcement_browsertest.js")) {
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(fake_latitude_,
                                                             fake_longitude_);
    if (IsNoWebview()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicNoWebview,
                                pwc::mojom::features::kPrivilegedWebContents},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kGlicNoWebview});
    }
  }
  ~GlicPermissionEnforcementBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kUseFakeUIForMediaStream);
  }

  bool IsNoWebview() const { return GetParam(); }
  bool IsWebview() const { return !IsNoWebview(); }

 protected:
  double fake_latitude_ = 1.23;
  double fake_longitude_ = 4.56;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicPermissionEnforcementBrowserTest,
                       testMicrophonePermissionTestDeny) {
  if (IsWebview()) {
    // TODO(crbug.com/409118577): Microphone permissions are not actually gated
    // by the microphone permission in WebView mode yet.
    GTEST_SKIP() << "crbug.com/409118577: Microphone permissions are not gated "
                    "in WebView mode";
  }
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicPermissionEnforcementBrowserTest,
                       testMicrophonePermissionTestAllow) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, true);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicPermissionEnforcementBrowserTest,
                       testTabContextPermissionTestDeny) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, false);
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicPermissionEnforcementBrowserTest,
                       testTabContextPermissionTestAllow) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, true);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if BUILDFLAG(IS_ANDROID)
// TODO(b/519278240): Enable once geolocation is fixed on android
#define MAYBE_testLocationPermissionTestDeny \
  DISABLED_testLocationPermissionTestDeny
#else
#define MAYBE_testLocationPermissionTestDeny testLocationPermissionTestDeny
#endif
IN_PROC_BROWSER_TEST_P(GlicPermissionEnforcementBrowserTest,
                       MAYBE_testLocationPermissionTestDeny) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if BUILDFLAG(IS_ANDROID)
// TODO(b/519278240): Enable once geolocation is fixed on android
#define MAYBE_testLocationPermissionTestAllow \
  DISABLED_testLocationPermissionTestAllow
#else
#define MAYBE_testLocationPermissionTestAllow testLocationPermissionTestAllow
#endif
IN_PROC_BROWSER_TEST_P(GlicPermissionEnforcementBrowserTest,
                       MAYBE_testLocationPermissionTestAllow) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, true);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicPermissionEnforcementBrowserTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "NoWebview" : "Webview";
    });

}  // namespace glic
