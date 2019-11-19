// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/prefs/pref_service.h"
#include "extensions/test/result_catcher.h"

// API tests for chrome.accessibilityFeatures API.
// Note that the API is implemented using preference API infrastructure.
// See preference_api.cc for the list of accessibility features exposed by the
// API and the related preferences.

namespace extensions {

namespace {

// Keys for data in the test config argument that will be set for the test app
// to use.
// The test that the app should run.
const char kTestNameKey[] = "testName";
// Key for list of features enabled when the test is initialized.
const char kEnabledFeaturesKey[] = "enabled";
// Key for list fo features disabled when the test is initialized.
const char kDisabledFeaturesKey[] = "disabled";

// A test extension path. The extension has only |accessibilityFeatures.read|
// permission.
const char kTestExtensionPathReadPermission[] =
    "accessibility_features/read_permission/";
// A test extension path. The extension has only |accessibilityFeatures.modify|
// permission.
const char kTestExtensionPathMofifyPermission[] =
    "accessibility_features/modify_permission/";

// Accessibility features API test.
// Tests are parameterized by whether the test extension is write-only (the
// parameter value is true) or read-only (the parameter value is false).
class AccessibilityFeaturesApiTest : public ExtensionApiTest,
                                     public testing::WithParamInterface<bool> {
 public:
  AccessibilityFeaturesApiTest() {}
  virtual ~AccessibilityFeaturesApiTest() {}

 protected:
  // Returns pref service to be used to initialize and later verify
  // accessibility preference values.
  PrefService* GetPrefs() { return browser()->profile()->GetPrefs(); }

  // Returns the path of the extension that should be used in a parameterized
  // test.
  std::string GetTestExtensionPath() const {
    if (GetParam())
      return kTestExtensionPathMofifyPermission;
    return kTestExtensionPathReadPermission;
  }

  // Whether a parameterized test should have been able to modify accessibility
  // preferences (i.e. whether the test extension had modify permission).
  bool ShouldModifyingFeatureSucceed() const { return GetParam(); }

  // Returns preference path for accessibility features as defined by the API.
  const char* GetPrefForFeature(const std::string& feature) {
    if (feature == "spokenFeedback")
      return ash::prefs::kAccessibilitySpokenFeedbackEnabled;
    if (feature == "largeCursor")
      return ash::prefs::kAccessibilityLargeCursorEnabled;
    if (feature == "stickyKeys")
      return ash::prefs::kAccessibilityStickyKeysEnabled;
    if (feature == "highContrast")
      return ash::prefs::kAccessibilityHighContrastEnabled;
    if (feature == "screenMagnifier")
      return ash::prefs::kAccessibilityScreenMagnifierEnabled;
    if (feature == "autoclick")
      return ash::prefs::kAccessibilityAutoclickEnabled;
    if (feature == "virtualKeyboard")
      return ash::prefs::kAccessibilityVirtualKeyboardEnabled;
    return NULL;
  }

  // Initializes preferences before running the test extension.
  // |prefs| Pref service which should be initializzed.
  // |enabled_features| List of boolean preference whose value should be set to
  //     true.
  // |disabled_features| List of boolean preferences whose value should be set
  //     to false.
  bool InitPrefServiceForTest(
      PrefService* prefs,
      const std::vector<std::string>& enabled_features,
      const std::vector<std::string>& disabled_features) {
    for (size_t i = 0; i < enabled_features.size(); ++i) {
      const char* const pref_name = GetPrefForFeature(enabled_features[i]);
      EXPECT_TRUE(pref_name) << "Invalid feature " << enabled_features[i];
      if (!pref_name)
        return false;
      prefs->SetBoolean(pref_name, true);
    }

    for (size_t i = 0; i < disabled_features.size(); ++i) {
      const char* const pref_name = GetPrefForFeature(disabled_features[i]);
      EXPECT_TRUE(pref_name) << "Invalid feature " << disabled_features[i];
      if (!pref_name)
        return false;
      prefs->SetBoolean(pref_name, false);
    }
    return true;
  }

  // Verifies that preferences have the expected value.
  // |prefs| The pref service to be verified.
  // |enabled_features| The list of boolean preferences whose value should be
  //     true.
  // |disabled_features| The list of boolean preferences whose value should be
  //     false.
  void VerifyPrefServiceState(
      PrefService* prefs,
      const std::vector<std::string>& enabled_features,
      const std::vector<std::string>& disabled_features) {
    for (size_t i = 0; i < enabled_features.size(); ++i) {
      const char* const pref_name = GetPrefForFeature(enabled_features[i]);
      ASSERT_TRUE(pref_name) << "Invalid feature " << enabled_features[i];
      ASSERT_TRUE(prefs->GetBoolean(pref_name));
    }

    for (size_t i = 0; i < disabled_features.size(); ++i) {
      const char* const pref_name = GetPrefForFeature(disabled_features[i]);
      ASSERT_TRUE(pref_name) << "Invalid feature " << disabled_features[i];
      ASSERT_FALSE(prefs->GetBoolean(pref_name));
    }
  }

  // Given the test name and list of enabled and disabled features, generates
  // and sets the JSON string that should be given to the test extension as
  // test configuration.
  // The result is saved to |result|. The return value is whether the test
  // argument was successfully generated.
  bool GenerateTestArg(const std::string& test_name,
                       const std::vector<std::string>& enabled_features,
                       const std::vector<std::string>& disabled_features,
                       std::string* result) {
    base::DictionaryValue test_arg;
    test_arg.SetString(kTestNameKey, test_name);

    std::unique_ptr<base::ListValue> enabled_list(new base::ListValue);
    for (size_t i = 0; i < enabled_features.size(); ++i)
      enabled_list->AppendString(enabled_features[i]);
    test_arg.Set(kEnabledFeaturesKey, std::move(enabled_list));

    std::unique_ptr<base::ListValue> disabled_list(new base::ListValue);
    for (size_t i = 0; i < disabled_features.size(); ++i)
      disabled_list->AppendString(disabled_features[i]);
    test_arg.Set(kDisabledFeaturesKey, std::move(disabled_list));

    return base::JSONWriter::Write(test_arg, result);
  }
};

INSTANTIATE_TEST_SUITE_P(AccessibilityFeatureaApiTestInstantiatePermission,
                         AccessibilityFeaturesApiTest,
                         testing::Bool());

// Tests that an extension with read permission can read accessibility features
// state, while an extension that doesn't have the permission cannot.
IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, Get) {
  // WARNING: Make sure that spoken feedback is not among enabled_features
  // (see |Set| test for the reason).
  std::vector<std::string> enabled_features;
  enabled_features.push_back("largeCursor");
  enabled_features.push_back("stickyKeys");
  enabled_features.push_back("highContrast");

  std::vector<std::string> disabled_features;
  disabled_features.push_back("spokenFeedback");
  disabled_features.push_back("screenMagnifier");
  disabled_features.push_back("autoclick");
  disabled_features.push_back("virtualKeyboard");

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg(
      "getterTest", enabled_features, disabled_features, &test_arg));
  EXPECT_TRUE(
      RunPlatformAppTestWithArg(GetTestExtensionPath(), test_arg.c_str()))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, PRE_Get_ComponentApp) {
  EXPECT_FALSE(RunPlatformAppTestWithFlags(GetTestExtensionPath(), "{}",
                                           kFlagLoadAsComponent))
      << message_;
}

// A regression test for https://crbug.com/454513. Ensure that loading a
// component extension with the same version as has previously loaded, correctly
// sets up access to accessibility prefs. Otherwise,this is the same as the
// |Get| test.
IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, Get_ComponentApp) {
  // WARNING: Make sure that spoken feedback is not among enabled_features
  // (see |AccessibilityFeaturesApiTest.Set| test for the reason).
  std::vector<std::string> enabled_features = {"largeCursor", "stickyKeys",
                                               "highContrast"};

  std::vector<std::string> disabled_features = {
      "spokenFeedback", "screenMagnifier", "autoclick", "virtualKeyboard"};

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg("getterTest", enabled_features, disabled_features,
                              &test_arg));
  EXPECT_TRUE(RunPlatformAppTestWithFlags(
      GetTestExtensionPath(), test_arg.c_str(), kFlagLoadAsComponent))
      << message_;
}

// Tests that an extension with modify permission can modify accessibility
// features, while an extension that doesn't have the permission can't.
IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, Set) {
  // WARNING: Make sure that spoken feedback does not get enabled at this point
  // (before the test app is loaded), as that may break the test:
  // |RunPlatformAppTestWithArg| waits for the test extension to load by
  // waiting for EXTENSION_LOADED notification to be observed. It also assumes
  // that there is only one extension being loaded during this time (it finishes
  // when the first notification is seen). Enabling spoken feedback here would
  // break this assumption as it would induce loading of ChromeVox extension.
  std::vector<std::string> enabled_features;
  enabled_features.push_back("stickyKeys");
  enabled_features.push_back("virtualKeyboard");

  std::vector<std::string> disabled_features;
  disabled_features.push_back("spokenFeedback");
  disabled_features.push_back("largeCursor");
  disabled_features.push_back("highContrast");
  disabled_features.push_back("screenMagnifier");
  disabled_features.push_back("autoclick");

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg(
      "setterTest", enabled_features, disabled_features, &test_arg));

  // The test extension attempts to flip all feature values.
  ASSERT_TRUE(
      RunPlatformAppTestWithArg(GetTestExtensionPath(), test_arg.c_str()))
      << message_;

  // The test tries to flip the feature states.
  if (ShouldModifyingFeatureSucceed()) {
    VerifyPrefServiceState(GetPrefs(), disabled_features, enabled_features);
  } else {
    VerifyPrefServiceState(GetPrefs(), enabled_features, disabled_features);
  }
}

// Tests that an extension with read permission is notified when accessibility
// features change.
IN_PROC_BROWSER_TEST_F(AccessibilityFeaturesApiTest, ObserveFeatures) {
  // WARNING: Make sure that spoken feedback is not among enabled_features
  // (see |Set| test for the reason).
  std::vector<std::string> enabled_features;
  enabled_features.push_back("largeCursor");
  enabled_features.push_back("stickyKeys");
  enabled_features.push_back("highContrast");

  std::vector<std::string> disabled_features;
  disabled_features.push_back("screenMagnifier");

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg(
      "observerTest", enabled_features, disabled_features, &test_arg));

  // The test extension is supposed to report result twice when runnign this
  // test. First time when in initializes it's feature listeners, and second
  // time, when gets all expected events. This is done so the extension is
  // running when the accessibility features are flipped; oterwise, the
  // extension may not see events.
  ASSERT_TRUE(RunPlatformAppTestWithArg(kTestExtensionPathReadPermission,
                                        test_arg.c_str()))
      << message_;

  // This should flip all features.
  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), disabled_features, enabled_features));

  // Catch the second result notification sent by the test extension.
  ResultCatcher result_catcher;
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace

}  // namespace extensions
