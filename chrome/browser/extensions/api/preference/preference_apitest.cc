// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

using CookieControlsMode = content_settings::CookieControlsMode;

class ExtensionPreferenceApiTest : public extensions::ExtensionApiTest {
 protected:
  ExtensionPreferenceApiTest() : profile_(nullptr) {}

  void SetCookieControlsMode(PrefService* prefs, CookieControlsMode mode) {
    prefs->SetInteger(prefs::kCookieControlsMode, static_cast<int>(mode));
  }

  CookieControlsMode GetCookieControlsMode(PrefService* prefs) {
    return static_cast<CookieControlsMode>(
        prefs->GetInteger(prefs::kCookieControlsMode));
  }

  void CheckPreferencesSet() {
    PrefService* prefs = profile_->GetPrefs();
    const PrefService::Preference* pref =
        prefs->FindPreference(prefs::kCookieControlsMode);
    ASSERT_TRUE(pref);
    EXPECT_TRUE(pref->IsExtensionControlled());
    EXPECT_TRUE(
        prefs->GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
    EXPECT_TRUE(prefs->GetBoolean(autofill::prefs::kAutofillEnabledDeprecated));
    EXPECT_TRUE(prefs->GetBoolean(autofill::prefs::kAutofillCreditCardEnabled));
    EXPECT_TRUE(prefs->GetBoolean(autofill::prefs::kAutofillProfileEnabled));
    EXPECT_EQ(CookieControlsMode::kOff, GetCookieControlsMode(prefs));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableHyperlinkAuditing));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableReferrers));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kOfferTranslateEnabled));
    EXPECT_EQ(chrome_browser_net::NETWORK_PREDICTION_DEFAULT,
              prefs->GetInteger(prefs::kNetworkPredictionOptions));
    EXPECT_TRUE(
        prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
  }

  void CheckPreferencesCleared() {
    PrefService* prefs = profile_->GetPrefs();
    const PrefService::Preference* pref =
        prefs->FindPreference(prefs::kCookieControlsMode);
    ASSERT_TRUE(pref);
    EXPECT_FALSE(pref->IsExtensionControlled());
    EXPECT_FALSE(
        prefs->GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
    EXPECT_FALSE(
        prefs->GetBoolean(autofill::prefs::kAutofillEnabledDeprecated));
    EXPECT_FALSE(
        prefs->GetBoolean(autofill::prefs::kAutofillCreditCardEnabled));
    EXPECT_FALSE(prefs->GetBoolean(autofill::prefs::kAutofillProfileEnabled));
    EXPECT_EQ(CookieControlsMode::kBlockThirdParty,
              GetCookieControlsMode(prefs));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kEnableHyperlinkAuditing));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kEnableReferrers));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kOfferTranslateEnabled));
    EXPECT_EQ(chrome_browser_net::NETWORK_PREDICTION_NEVER,
              prefs->GetInteger(prefs::kNetworkPredictionOptions));
    EXPECT_FALSE(
        prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
  }

  // Verifies whether the boolean |preference| has the |expected_value| and is
  // |expected_controlled| by an extension.

  void VerifyPrefValueAndControlledState(const std::string& preference,
                                         const base::Value& expected_value,
                                         bool expected_controlled) {
    SCOPED_TRACE(preference);

    PrefService* prefs = profile_->GetPrefs();
    const PrefService::Preference* pref = prefs->FindPreference(preference);
    ASSERT_TRUE(pref);
    const base::Value* actual_value = pref->GetValue();
    EXPECT_EQ(expected_value.type(), actual_value->type());

    EXPECT_EQ(expected_value, *actual_value);
    EXPECT_EQ(expected_controlled, pref->IsExtensionControlled());
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    // The browser might get closed later (and therefore be destroyed), so we
    // save the profile.
    profile_ = browser()->profile();

    // Closing the last browser window also releases a module reference. Make
    // sure it's not the last one, so the message loop doesn't quit
    // unexpectedly.
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
  }

  void TearDownOnMainThread() override {
    // BrowserProcess::Shutdown() needs to be called in a message loop, so we
    // post a task to release the keep alive, then run the message loop.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&std::unique_ptr<ScopedKeepAlive>::reset,
                                  base::Unretained(&keep_alive_), nullptr));
    content::RunAllPendingInMessageLoop();

    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  Profile* profile_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, Standard) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(embedder_support::kAlternateErrorPagesEnabled, false);
  prefs->SetBoolean(autofill::prefs::kAutofillEnabledDeprecated, false);
  prefs->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, false);
  prefs->SetBoolean(autofill::prefs::kAutofillProfileEnabled, false);
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);
  prefs->SetBoolean(prefs::kEnableHyperlinkAuditing, false);
  prefs->SetBoolean(prefs::kEnableReferrers, false);
  prefs->SetBoolean(prefs::kOfferTranslateEnabled, false);
  prefs->SetInteger(prefs::kNetworkPredictionOptions,
                    chrome_browser_net::NETWORK_PREDICTION_NEVER);
  prefs->SetBoolean(password_manager::prefs::kCredentialsEnableService, false);
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  prefs->SetBoolean(prefs::kSearchSuggestEnabled, false);
  prefs->SetBoolean(prefs::kWebRTCMultipleRoutesEnabled, false);
  prefs->SetBoolean(prefs::kWebRTCNonProxiedUdpEnabled, false);
  prefs->SetString(prefs::kWebRTCIPHandlingPolicy,
                   blink::kWebRTCIPHandlingDefaultPublicInterfaceOnly);

  const char kExtensionPath[] = "preference/standard";

  EXPECT_TRUE(RunExtensionTest(kExtensionPath)) << message_;
  CheckPreferencesSet();

  // The settings should not be reset when the extension is reloaded.
  ReloadExtension(last_loaded_extension_id());
  CheckPreferencesSet();

  // Uninstalling and installing the extension (without running the test that
  // calls the extension API) should clear the settings.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(profile_), last_loaded_extension_id());
  UninstallExtension(last_loaded_extension_id());
  observer.WaitForExtensionUninstalled();
  CheckPreferencesCleared();

  LoadExtension(test_data_dir_.AppendASCII(kExtensionPath));
  CheckPreferencesCleared();
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, PersistentIncognito) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(
      RunExtensionTestIncognito("preference/persistent_incognito")) <<
      message_;

  // Setting an incognito preference should not create an incognito profile.
  EXPECT_FALSE(profile_->HasPrimaryOTRProfile());

  PrefService* otr_prefs = profile_->GetPrimaryOTRProfile()->GetPrefs();
  auto* otr_pref = otr_prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(otr_pref);
  EXPECT_TRUE(otr_pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kBlockThirdParty,
            GetCookieControlsMode(otr_prefs));

  auto* pref = prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kOff, GetCookieControlsMode(prefs));
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, IncognitoDisabled) {
  EXPECT_FALSE(RunExtensionTest("preference/persistent_incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, SessionOnlyIncognito) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(
      RunExtensionTestIncognito("preference/session_only_incognito")) <<
      message_;

  EXPECT_TRUE(profile_->HasPrimaryOTRProfile());

  PrefService* otr_prefs = profile_->GetPrimaryOTRProfile()->GetPrefs();
  auto* otr_pref = otr_prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(otr_pref);
  EXPECT_TRUE(otr_pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kOff, GetCookieControlsMode(otr_prefs));

  auto* pref = prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kOff, GetCookieControlsMode(prefs));
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, Clear) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);

  EXPECT_TRUE(RunExtensionTest("preference/clear")) << message_;

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kBlockThirdParty, GetCookieControlsMode(prefs));
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, OnChange) {
  EXPECT_TRUE(RunExtensionTestIncognito("preference/onchange")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, OnChangeSplit) {
  extensions::ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile_);
  extensions::ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(profile_->GetPrimaryOTRProfile());

  // Open an incognito window.
  OpenURLOffTheRecord(profile_, GURL("chrome://newtab/"));

  // changeDefault listeners.
  ExtensionTestMessageListener listener1("changeDefault regular ready", true);
  ExtensionTestMessageListener listener_incognito1(
      "changeDefault incognito ready", true);

  // changeIncognitoOnly listeners.
  ExtensionTestMessageListener listener2(
      "changeIncognitoOnly regular ready", true);
  ExtensionTestMessageListener listener_incognito2(
      "changeIncognitoOnly incognito ready", true);
  ExtensionTestMessageListener listener3(
      "changeIncognitoOnly regular listening", true);
  ExtensionTestMessageListener listener_incognito3(
      "changeIncognitoOnly incognito pref set", false);

  // changeDefaultOnly listeners.
  ExtensionTestMessageListener listener4(
      "changeDefaultOnly regular ready", true);
  ExtensionTestMessageListener listener_incognito4(
      "changeDefaultOnly incognito ready", true);
  ExtensionTestMessageListener listener5(
      "changeDefaultOnly regular pref set", false);
  ExtensionTestMessageListener listener_incognito5(
      "changeDefaultOnly incognito listening", true);

  // changeIncognitoOnlyBack listeners.
  ExtensionTestMessageListener listener6(
      "changeIncognitoOnlyBack regular ready", true);
  ExtensionTestMessageListener listener_incognito6(
      "changeIncognitoOnlyBack incognito ready", true);
  ExtensionTestMessageListener listener7(
      "changeIncognitoOnlyBack regular listening", true);
  ExtensionTestMessageListener listener_incognito7(
      "changeIncognitoOnlyBack incognito pref set", false);

  // clearIncognito listeners.
  ExtensionTestMessageListener listener8(
      "clearIncognito regular ready", true);
  ExtensionTestMessageListener listener_incognito8(
      "clearIncognito incognito ready", true);
  ExtensionTestMessageListener listener9(
      "clearIncognito regular listening", true);
  ExtensionTestMessageListener listener_incognito9(
      "clearIncognito incognito pref cleared", false);

  // clearDefault listeners.
  ExtensionTestMessageListener listener10(
      "clearDefault regular ready", true);
  ExtensionTestMessageListener listener_incognito10(
      "clearDefault incognito ready", true);

  base::FilePath extension_data_dir =
      test_data_dir_.AppendASCII("preference").AppendASCII("onchange_split");
  ASSERT_TRUE(LoadExtensionIncognito(extension_data_dir));

  // Test 1 - changeDefault
  EXPECT_TRUE(listener1.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito1.WaitUntilSatisfied()); // Incognito ready
  listener1.Reply("ok");
  listener_incognito1.Reply("ok");

  // Test 2 - changeIncognitoOnly
  EXPECT_TRUE(listener2.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito2.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener3.WaitUntilSatisfied()); // Regular listening
  listener2.Reply("ok");
  listener_incognito2.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito3.WaitUntilSatisfied());
  listener3.Reply("ok");

  // Test 3 - changeDefaultOnly
  EXPECT_TRUE(listener4.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito4.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener_incognito5.WaitUntilSatisfied()); // Incognito listening
  listener4.Reply("ok");
  listener_incognito4.Reply("ok");
  // Regular preference set - notify the incognito listener
  EXPECT_TRUE(listener5.WaitUntilSatisfied());
  listener_incognito5.Reply("ok");

  // Test 4 - changeIncognitoOnlyBack
  EXPECT_TRUE(listener6.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito6.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener7.WaitUntilSatisfied()); // Regular listening
  listener6.Reply("ok");
  listener_incognito6.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito7.WaitUntilSatisfied());
  listener7.Reply("ok");

  // Test 5 - clearIncognito
  EXPECT_TRUE(listener8.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito8.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener9.WaitUntilSatisfied()); // Regular listening
  listener8.Reply("ok");
  listener_incognito8.Reply("ok");
  // Incognito preference cleared -- notify the regular listener
  EXPECT_TRUE(listener_incognito9.WaitUntilSatisfied());
  listener9.Reply("ok");

  // Test 6 - clearDefault
  EXPECT_TRUE(listener10.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito10.WaitUntilSatisfied()); // Incognito ready
  listener10.Reply("ok");
  listener_incognito10.Reply("ok");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher_incognito.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest,
                       OnChangeSplitWithNoOTRProfile) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener loaded_incognito_test_listener(
      "incognito loaded", false);

  ExtensionTestMessageListener change_pref_listener("change pref value", false);

  ASSERT_TRUE(
      LoadExtensionIncognito(test_data_dir_.AppendASCII("preference")
                                 .AppendASCII("onchange_split_regular_only")));

  ASSERT_TRUE(change_pref_listener.WaitUntilSatisfied());
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(loaded_incognito_test_listener.was_satisfied());
  EXPECT_FALSE(profile_->HasPrimaryOTRProfile());
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest,
                       OnChangeSplitWithoutIncognitoAccess) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);

  // Open an incognito window.
  OpenURLOffTheRecord(profile_, GURL("chrome://newtab/"));
  EXPECT_TRUE(profile_->HasPrimaryOTRProfile());

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener loaded_incognito_test_listener(
      "incognito loaded", false);

  ExtensionTestMessageListener change_pref_listener("change pref value", false);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("preference")
                                .AppendASCII("onchange_split_regular_only")));

  ASSERT_TRUE(change_pref_listener.WaitUntilSatisfied());
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(loaded_incognito_test_listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, DataReductionProxy) {
  EXPECT_TRUE(RunExtensionTest("preference/data_reduction_proxy")) <<
      message_;
}

// Tests the behavior of the Safe Browsing API as described in
// crbug.com/1064722.
IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, SafeBrowsing_SetTrue) {
  ExtensionTestMessageListener listener_true("set to true",
                                             /* will_reply */ true);
  ExtensionTestMessageListener listener_clear("cleared", /* will_reply */ true);
  ExtensionTestMessageListener listener_false("set to false",
                                              /* will_reply */ true);
  ExtensionTestMessageListener listener_done("done", /* will_reply */ false);

  const base::FilePath extension_path =
      test_data_dir_.AppendASCII("preference").AppendASCII("safe_browsing");
  const extensions::Extension* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);

  // Step 1. of the test sets the API to TRUE.
  // Both preferences are now controlled by extension. |kSafeBrowsingEnabled| is
  // set to TRUE, while |kSafeBrowsingEnhanced| is always FALSE.
  ASSERT_TRUE(listener_true.WaitUntilSatisfied());
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnabled,
                                    base::Value(true),
                                    /* expected_controlled */ true);
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnhanced,
                                    base::Value(false),
                                    /* expected_controlled */ true);
  listener_true.Reply("ok");

  // Step 2. of the test clears the value.
  // Neither preference is now controlled by extension, and they take on their
  // default values - TRUE and FALSE, respectively.
  ASSERT_TRUE(listener_clear.WaitUntilSatisfied());
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnabled,
                                    base::Value(true),
                                    /* expected_controlled */ false);
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnhanced,
                                    base::Value(false),
                                    /* expected_controlled */ false);
  listener_clear.Reply("ok");

  // Step 3. of the test sets the API to FALSE.
  // Both preferences are now controlled by extension. |kSafeBrowsingEnabled| is
  // set to FALSE, and |kSafeBrowsingEnhanced| is also FALSE.
  ASSERT_TRUE(listener_false.WaitUntilSatisfied());
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnabled,
                                    base::Value(false),
                                    /* expected_controlled */ true);
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnhanced,
                                    base::Value(false),
                                    /* expected_controlled */ true);
  listener_false.Reply("ok");

  // Step 4. of the test uninstalls the extension.
  // Neither preference is now controlled by extension, and they take on their
  // default values - TRUE and FALSE, respectively.
  ASSERT_TRUE(listener_done.WaitUntilSatisfied());
  UninstallExtension(extension->id());
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnabled,
                                    base::Value(true),
                                    /* expected_controlled */ false);
  VerifyPrefValueAndControlledState(prefs::kSafeBrowsingEnhanced,
                                    base::Value(false),
                                    /* expected_controlled */ false);
}

// Tests the behavior of the ThirdPartyCookies preference API.
// kCookieControlsMode should be set to kOff/kBlockThirdParty if
// ThirdPartyCookiesAllowed is set to true/false by an extension.
IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, ThirdPartyCookiesAllowed) {
  ExtensionTestMessageListener listener_true("set to true",
                                             /* will_reply */ true);
  ExtensionTestMessageListener listener_clear("cleared", /* will_reply */ true);
  ExtensionTestMessageListener listener_false("set to false",
                                              /* will_reply */ true);
  ExtensionTestMessageListener listener_done("done", /* will_reply */ false);

  // Verify initial state.
  VerifyPrefValueAndControlledState(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(
          content_settings::CookieControlsMode::kIncognitoOnly)),
      /* expected_controlled */ false);

  const base::FilePath extension_path =
      test_data_dir_.AppendASCII("preference")
          .AppendASCII("third_party_cookies_allowed");
  const extensions::Extension* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);

  // Step 1. of the test sets the API to TRUE.
  ASSERT_TRUE(listener_true.WaitUntilSatisfied());
  VerifyPrefValueAndControlledState(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(content_settings::CookieControlsMode::kOff)),
      /* expected_controlled */ true);
  listener_true.Reply("ok");

  // Step 2. of the test clears the value.
  ASSERT_TRUE(listener_clear.WaitUntilSatisfied());
  VerifyPrefValueAndControlledState(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(
          content_settings::CookieControlsMode::kIncognitoOnly)),
      /* expected_controlled */ false);
  listener_clear.Reply("ok");

  // Step 3. of the test sets the API to FALSE.
  ASSERT_TRUE(listener_false.WaitUntilSatisfied());
  VerifyPrefValueAndControlledState(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)),
      /* expected_controlled */ true);
  listener_false.Reply("ok");

  // Step 4. of the test uninstalls the extension.
  ASSERT_TRUE(listener_done.WaitUntilSatisfied());
  UninstallExtension(extension->id());
  VerifyPrefValueAndControlledState(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(
          content_settings::CookieControlsMode::kIncognitoOnly)),
      /* expected_controlled */ false);
}
