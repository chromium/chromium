// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/android/glic_helper_android.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#endif

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
          /*enabled_features=*/{features::kGlicNoWebview, features::kGlicVoice},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicVoice},
          /*disabled_features=*/{features::kGlicNoWebview});
    }
  }
  ~GlicPermissionEnforcementBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kUseFakeUIForMediaStream);
  }

#if BUILDFLAG(IS_ANDROID)
  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();
    // Report the Android mic permission as granted, and decline Chrome's mic
    // dialog if shown (e.g. when the Glic microphone setting is off), so that
    // no real Android dialogs are shown.
    class GrantedMicPermissionUi : public MicPermissionUi {
     public:
      void ShowMicPermissionDialog(
          ui::WindowAndroid* window_android,
          base::OnceCallback<void(bool)> callback) override {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), false));
      }
      bool HasMicOsPermission(ui::WindowAndroid* window_android) override {
        return true;
      }
    };
    GlicKeyedServiceFactory::GetGlicKeyedService(GetProfile())
        ->SetMicPermissionUiForTesting(
            std::make_unique<GrantedMicPermissionUi>());
  }
#endif

  bool IsNoWebview() const { return GetParam(); }
  bool IsWebview() const { return !IsNoWebview(); }

 protected:
  double fake_latitude_ = 1.23;
  double fake_longitude_ = 4.56;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_ANDROID)
// TODO(b/568819844): Enable once the microphone setting gates mic access.
#define MAYBE_testMicrophonePermissionTestDeny \
  DISABLED_testMicrophonePermissionTestDeny
#else
#define MAYBE_testMicrophonePermissionTestDeny testMicrophonePermissionTestDeny
#endif
IN_PROC_BROWSER_TEST_P(GlicPermissionEnforcementBrowserTest,
                       MAYBE_testMicrophonePermissionTestDeny) {
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
