// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

namespace glic {

class GlicPermissionEnforcementBrowserTest : public GlicApiBrowserTest {
 public:
  GlicPermissionEnforcementBrowserTest()
      : GlicApiBrowserTest("./glic_permission_enforcement_browsertest.js") {
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(fake_latitude_,
                                                             fake_longitude_);
  }
  ~GlicPermissionEnforcementBrowserTest() override = default;

 protected:
  double fake_latitude_ = 1.23;
  double fake_longitude_ = 4.56;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementBrowserTest,
                       testAllTestsAreRegistered) {
  AssertAllTestsRegistered({"GlicPermissionEnforcementBrowserTest"});
}

// TODO(crbug.com/409118577): Microphone permissions are not actually gated by
// the microphone permission yet.
IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementBrowserTest,
                       DISABLED_testMicrophonePermissionTestDeny) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if BUILDFLAG(IS_ANDROID)
// TODO: Android does not support microphone input.
#define MAYBE_testMicrophonePermissionTestAllow \
  DISABLED_testMicrophonePermissionTestAllow
#else
#define MAYBE_testMicrophonePermissionTestAllow \
  testMicrophonePermissionTestAllow
#endif
IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementBrowserTest,
                       MAYBE_testMicrophonePermissionTestAllow) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, true);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementBrowserTest,
                       testTabContextPermissionTestDeny) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicTabContextEnabled, false);
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementBrowserTest,
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
IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementBrowserTest,
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
IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementBrowserTest,
                       MAYBE_testLocationPermissionTestAllow) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, true);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

}  // namespace glic
