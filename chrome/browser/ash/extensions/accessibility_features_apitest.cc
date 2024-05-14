// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
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
// Key for list of features disabled when the test is initialized.
const char kDisabledFeaturesKey[] = "disabled";

// A test extension path. The extension has only |accessibilityFeatures.read|
// permission.
const char kTestExtensionPathReadPermission[] =
    "accessibility_features/read_permission/";

// A test extension path. The extension has only |accessibilityFeatures.read|
// permission and has manifest v3.
const char kTestExtensionPathReadPermissionV3[] =
    "accessibility_features/mv3/read_permission/";

// A test extension path. The extension has only |accessibilityFeatures.modify|
// permission.
const char kTestExtensionPathModifyPermission[] =
    "accessibility_features/modify_permission/";

// A test extension path. The extension has only |accessibilityFeatures.modify|
// permission and has manifest v3.
const char kTestExtensionPathModifyPermissionV3[] =
    "accessibility_features/mv3/modify_permission/";

using ManifestVersion = ash::ManifestVersion;

enum class Permission { kWriteOnly, kReadOnly };

// A class used to define the parameters of a test case.
struct TestConfig {
  Permission permission;
  ManifestVersion version;
};

// Accessibility features API test.
// Tests are parameterized by the permission (write-only or read-only), as well
// as the manifest version (v2 or v3).
class AccessibilityFeaturesApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<TestConfig> {
 public:
  AccessibilityFeaturesApiTest() {}
  virtual ~AccessibilityFeaturesApiTest() {}

 protected:
  // Returns pref service to be used to initialize and later verify
  // accessibility preference values.
  PrefService* GetPrefs() { return browser()->profile()->GetPrefs(); }

  // Returns the path of the extension that should be used in a parameterized
  // test.
  const char* GetTestExtensionPath() const {
    Permission permission = GetParam().permission;
    ManifestVersion version = GetParam().version;
    if (version == ManifestVersion::kTwo &&
        permission == Permission::kWriteOnly) {
      return kTestExtensionPathModifyPermission;
    } else if (version == ManifestVersion::kTwo &&
               permission == Permission::kReadOnly) {
      return kTestExtensionPathReadPermission;
    } else if (version == ManifestVersion::kThree &&
               permission == Permission::kWriteOnly) {
      return kTestExtensionPathModifyPermissionV3;
    } else if (version == ManifestVersion::kThree &&
               permission == Permission::kReadOnly) {
      return kTestExtensionPathReadPermissionV3;
    }

    NOTREACHED_IN_MIGRATION();
    return "";
  }

  // Whether a parameterized test should have been able to modify accessibility
  // preferences (i.e. whether the test extension had modify permission).
  bool ShouldModifyingFeatureSucceed() const {
    return GetParam().permission == Permission::kWriteOnly;
  }

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
    if (feature == "caretHighlight")
      return ash::prefs::kAccessibilityCaretHighlightEnabled;
    if (feature == "cursorHighlight")
      return ash::prefs::kAccessibilityCursorHighlightEnabled;
    if (feature == "focusHighlight")
      return ash::prefs::kAccessibilityFocusHighlightEnabled;
    if (feature == "selectToSpeak")
      return ash::prefs::kAccessibilitySelectToSpeakEnabled;
    if (feature == "switchAccess")
      return ash::prefs::kAccessibilitySwitchAccessEnabled;
    if (feature == "cursorColor")
      return ash::prefs::kAccessibilityCursorColorEnabled;
    if (feature == "dockedMagnifier")
      return ash::prefs::kDockedMagnifierEnabled;
    if (feature == "dictation")
      return ash::prefs::kAccessibilityDictationEnabled;
    return nullptr;
  }

  // Initializes preferences before running the test extension.
  // |prefs| Pref service which should be initialized.
  // |enabled_features| List of boolean preference whose value should be set to
  //     true.
  // |disabled_features| List of boolean preferences whose value should be set
  //     to false.
  bool InitPrefServiceForTest(
      PrefService* prefs,
      const std::vector<std::string>& enabled_features,
      const std::vector<std::string>& disabled_features) {
    for (const auto& feature : enabled_features) {
      const char* const pref_name = GetPrefForFeature(feature);
      EXPECT_TRUE(pref_name) << "Invalid feature " << feature;
      if (!pref_name)
        return false;
      prefs->SetBoolean(pref_name, true);
    }

    for (const auto& feature : disabled_features) {
      const char* const pref_name = GetPrefForFeature(feature);
      EXPECT_TRUE(pref_name) << "Invalid feature " << feature;
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
    for (const auto& feature : enabled_features) {
      const char* const pref_name = GetPrefForFeature(feature);
      ASSERT_TRUE(pref_name) << "Invalid feature " << feature;
      ASSERT_TRUE(prefs->GetBoolean(pref_name));
    }

    for (const auto& feature : disabled_features) {
      const char* const pref_name = GetPrefForFeature(feature);
      ASSERT_TRUE(pref_name) << "Invalid feature " << feature;
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
    base::Value::Dict test_arg;
    test_arg.Set(kTestNameKey, test_name);

    base::Value::List enabled_list;
    for (const auto& feature : enabled_features)
      enabled_list.Append(feature);
    test_arg.Set(kEnabledFeaturesKey, std::move(enabled_list));

    base::Value::List disabled_list;
    for (const auto& feature : disabled_features)
      disabled_list.Append(feature);
    test_arg.Set(kDisabledFeaturesKey, std::move(disabled_list));

    return base::JSONWriter::Write(test_arg, result);
  }
};

INSTANTIATE_TEST_SUITE_P(AccessibilityFeaturesApiTestWritePermission,
                         AccessibilityFeaturesApiTest,
                         ::testing::Values(TestConfig{Permission::kWriteOnly,
                                                      ManifestVersion::kTwo}));

INSTANTIATE_TEST_SUITE_P(AccessibilityFeaturesApiTestReadPermission,
                         AccessibilityFeaturesApiTest,
                         ::testing::Values(TestConfig{Permission::kReadOnly,
                                                      ManifestVersion::kTwo}));

INSTANTIATE_TEST_SUITE_P(AccessibilityFeaturesApiTestWritePermissionV3,
                         AccessibilityFeaturesApiTest,
                         ::testing::Values(TestConfig{
                             Permission::kWriteOnly, ManifestVersion::kThree}));

INSTANTIATE_TEST_SUITE_P(AccessibilityFeaturesApiTestReadPermissionV3,
                         AccessibilityFeaturesApiTest,
                         ::testing::Values(TestConfig{
                             Permission::kReadOnly, ManifestVersion::kThree}));

// Tests that an extension with read permission can read accessibility features
// state, while an extension that doesn't have the permission cannot.
IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, Get) {
  // WARNING: Make sure that features which load Chrome extension are not among
  // enabled_features (see |Set| test for the reason).
  std::vector<std::string> enabled_features = {
      "cursorColor",
      "cursorHighlight",
      "highContrast",
      "largeCursor",
      "stickyKeys",
  };

  std::vector<std::string> disabled_features = {
      "autoclick",
      "caretHighlight",
      "dockedMagnifier",
      "focusHighlight",
      "screenMagnifier",
      "selectToSpeak",
      "spokenFeedback",
      "switchAccess",
      "virtualKeyboard",
  };

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg("getterTest", enabled_features, disabled_features,
                              &test_arg));

  bool is_mv2 = GetParam().version == ManifestVersion::kTwo;
  EXPECT_TRUE(RunExtensionTest(
      GetTestExtensionPath(),
      {.custom_arg = test_arg.c_str(), .launch_as_platform_app = is_mv2}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, PRE_Get_ComponentApp) {
  bool is_mv2 = GetParam().version == ManifestVersion::kTwo;
  EXPECT_FALSE(
      RunExtensionTest(GetTestExtensionPath(),
                       {.custom_arg = "{}", .launch_as_platform_app = is_mv2},
                       {.load_as_component = is_mv2}))
      << message_;
}

// A regression test for https://crbug.com/454513. Ensure that loading a
// component extension with the same version as has previously loaded, correctly
// sets up access to accessibility prefs. Otherwise,this is the same as the
// |Get| test.
IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, Get_ComponentApp) {
  // WARNING: Make sure that features which load Chrome extension are not among
  // enabled_features (see |Set| test for the reason).
  std::vector<std::string> enabled_features = {
      "cursorHighlight",
      "dockedMagnifier",
      "highContrast",
      "largeCursor",
      "stickyKeys",
  };

  std::vector<std::string> disabled_features = {
      "autoclick",
      "caretHighlight",
      "cursorColor",
      "focusHighlight",
      "screenMagnifier",
      "selectToSpeak",
      "spokenFeedback",
      "switchAccess",
      "virtualKeyboard",
  };

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg("getterTest", enabled_features, disabled_features,
                              &test_arg));

  bool is_mv2 = GetParam().version == ManifestVersion::kTwo;
  EXPECT_TRUE(RunExtensionTest(
      GetTestExtensionPath(),
      {.custom_arg = test_arg.c_str(), .launch_as_platform_app = is_mv2},
      {.load_as_component = is_mv2}))
      << message_;
}

// Tests that an extension with modify permission can modify accessibility
// features, while an extension that doesn't have the permission can't.
IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, Set) {
  // WARNING: Make sure that features which load Chrome extension are not
  // enabled at this point (before the test app is loaded), as that may break
  // the test:
  // |RunPlatformAppTestWithArg| waits for the test extension to load by
  // waiting for EXTENSION_LOADED notification to be observed. It also assumes
  // that there is only one extension being loaded during this time (it finishes
  // when the first notification is seen). Enabling spoken feedback, select to
  // speak, autoclick, or switch access here would break this assumption as it
  // would induce loading of Chrome extension.
  std::vector<std::string> enabled_features = {
      "caretHighlight",
      "cursorColor",
      "focusHighlight",
      "stickyKeys",
  };

  std::vector<std::string> disabled_features = {
      "autoclick",
      "cursorHighlight",
      "dockedMagnifier",
      "highContrast",
      "largeCursor",
      "screenMagnifier",
      "selectToSpeak",
      "spokenFeedback",
      "switchAccess",
      "virtualKeyboard",
  };

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg("setterTest", enabled_features, disabled_features,
                              &test_arg));
  bool is_mv2 = GetParam().version == ManifestVersion::kTwo;
  // The test extension attempts to flip all feature values.
  ASSERT_TRUE(RunExtensionTest(
      GetTestExtensionPath(),
      {.custom_arg = test_arg.c_str(), .launch_as_platform_app = is_mv2}))
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
IN_PROC_BROWSER_TEST_P(AccessibilityFeaturesApiTest, ObserveFeatures) {
  // WARNING: Make sure that features which load Chrome extension are not among
  // enabled_features (see |Set| test for the reason).
  std::vector<std::string> enabled_features = {
      "caretHighlight",
      "cursorColor",
      "focusHighlight",
      "stickyKeys",
  };

  std::vector<std::string> disabled_features = {
      "autoclick",
      "cursorHighlight",
      "dockedMagnifier",
      "highContrast",
      "largeCursor",
      "screenMagnifier",
      "selectToSpeak",
      "spokenFeedback",
      "switchAccess",
      "virtualKeyboard",
  };

  ASSERT_TRUE(
      InitPrefServiceForTest(GetPrefs(), enabled_features, disabled_features));

  std::string test_arg;
  ASSERT_TRUE(GenerateTestArg("observerTest", enabled_features,
                              disabled_features, &test_arg));

  // The test extension is supposed to report result twice when running this
  // test. First time when in initializes it's feature listeners, and second
  // time, when gets all expected events. This is done so the extension is
  // running when the accessibility features are flipped; otherwise, the
  // extension may not see events.

  bool is_mv2 = GetParam().version == ManifestVersion::kTwo;
  const char* extension_path = is_mv2 ? kTestExtensionPathReadPermission
                                      : kTestExtensionPathReadPermissionV3;
  ASSERT_TRUE(RunExtensionTest(
      extension_path,
      {.custom_arg = test_arg.c_str(), .launch_as_platform_app = is_mv2}))
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
