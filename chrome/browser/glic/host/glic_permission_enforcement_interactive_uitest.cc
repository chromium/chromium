// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interactive_test.h"

namespace glic {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);

class GlicPermissionEnforcementUiTest : public test::InteractiveGlicTest {
 public:
  GlicPermissionEnforcementUiTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(
                fake_latitude_,
                fake_longitude_)) {}
  ~GlicPermissionEnforcementUiTest() override = default;
  double fake_latitude() const { return fake_latitude_; }
  double fake_longitude() const { return fake_longitude_; }

 protected:
  // The values used for the position override.
  double fake_latitude_ = 1.23;
  double fake_longitude_ = 4.56;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

// TODO(crbug.com/409118577): Microphone permissions are not actually gated by
// the microphone permission yet.
IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementUiTest,
                       DISABLED_MicrophonePermissionTestDeny) {
  const InteractiveBrowserTest::DeepQuery kAudioCaptureStart = {
      "#audioCapStart"};
  const InteractiveBrowserTest::DeepQuery kAudioCaptureStop = {"#audioCapStop"};
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicMicrophoneEnabled, false);
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      ClickMockGlicElement(kAudioCaptureStart),
      WaitForJsResult(test::kGlicContentsElementId,
                      "() => document.querySelector('#audioStatus').innerText",
                      "Caught error: NotAllowedError: Permission denied"));
}

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementUiTest,
                       MicrophonePermissionTestAllow) {
  const InteractiveBrowserTest::DeepQuery kAudioCaptureStart = {
      "#audioCapStart"};
  const InteractiveBrowserTest::DeepQuery kAudioCaptureStop = {"#audioCapStop"};
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicMicrophoneEnabled, true);
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      ClickMockGlicElement(kAudioCaptureStart),
      WaitForJsResult(test::kGlicContentsElementId,
                      "() => document.querySelector('#audioStatus').innerText",
                      "Recording..."),
      ClickMockGlicElement(kAudioCaptureStop),
      WaitForJsResult(test::kGlicContentsElementId,
                      "() => document.querySelector('#audioStatus').innerText",
                      "Recording Stopped"));
}

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementUiTest,
                       TabContextPermissionTestDeny) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  const InteractiveBrowserTest::DeepQuery kContextToggle = {"#getpagecontext"};
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicTabContextEnabled, false);
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId, embedded_test_server()->GetURL("/")),
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      ClickMockGlicElement(kContextToggle),
      WaitForJsResult(
          test::kGlicContentsElementId,
          "() => document.querySelector('#getPageContextStatus').innerText",
          "Error getting page context: Error: tabContext failed: permission "
          "denied: context permission not enabled"));
}

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementUiTest,
                       TabContextPermissionTestAllow) {
  const InteractiveBrowserTest::DeepQuery kContextToggle = {"#getpagecontext"};
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicTabContextEnabled, true);
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId, embedded_test_server()->GetURL("/")),
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      ClickMockGlicElement(kContextToggle),
      WaitForJsResult(
          test::kGlicContentsElementId,
          "() => document.querySelector('#getPageContextStatus').innerText",
          "Finished Get Page Context."));
}

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementUiTest,
                       LocationPermissionTestDeny) {
  const InteractiveBrowserTest::DeepQuery kGetLocationButton = {"#getlocation"};
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicGeolocationEnabled, false);
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      ClickMockGlicElement(kGetLocationButton),
      WaitForJsResult(
          test::kGlicContentsElementId,
          "() => document.querySelector('#locationStatus').innerText",
          "Permission Denied."));
}

IN_PROC_BROWSER_TEST_F(GlicPermissionEnforcementUiTest,
                       LocationPermissionTestAllow) {
  const InteractiveBrowserTest::DeepQuery kGetLocationButton = {"#getlocation"};
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicGeolocationEnabled, true);
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      ClickMockGlicElement(kGetLocationButton),
      WaitForJsResult(
          test::kGlicContentsElementId,
          "() => document.querySelector('#locationStatus').innerText",
          "Location Received."));
}
}  // namespace glic
