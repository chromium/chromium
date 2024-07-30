// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/preloading/preloading_prefs.h"
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
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

using CookieControlsMode = content_settings::CookieControlsMode;

using ContextType = extensions::ExtensionBrowserTest::ContextType;

class ExtensionPreferenceApiTest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionPreferenceApiTest(const ExtensionPreferenceApiTest&) = delete;
  ExtensionPreferenceApiTest& operator=(const ExtensionPreferenceApiTest&) =
      delete;

 protected:
  ExtensionPreferenceApiTest() : ExtensionApiTest(GetParam()) {}
  ~ExtensionPreferenceApiTest() override = default;

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
    EXPECT_EQ(GetCookieControlsMode(prefs), CookieControlsMode::kOff);
    EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableHyperlinkAuditing));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableReferrers));
    EXPECT_TRUE(prefs->GetBoolean(translate::prefs::kOfferTranslateEnabled));
    EXPECT_EQ(static_cast<int>(prefetch::NetworkPredictionOptions::kDefault),
              prefs->GetInteger(prefetch::prefs::kNetworkPredictionOptions));
    EXPECT_TRUE(
        prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
    VerifyPrefValueAndControlledState(prefs::kPrivacySandboxM1TopicsEnabled,
                                      base::Value(false),
                                      /* expected_controlled */ true);
    VerifyPrefValueAndControlledState(prefs::kPrivacySandboxM1FledgeEnabled,
                                      base::Value(false),
                                      /* expected_controlled */ true);
    VerifyPrefValueAndControlledState(
        prefs::kPrivacySandboxM1AdMeasurementEnabled, base::Value(false),
        /* expected_controlled */ true);
    VerifyPrefValueAndControlledState(
        prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, base::Value(false),
        /* expected_controlled */ true);
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
    EXPECT_EQ(GetCookieControlsMode(prefs),
              CookieControlsMode::kBlockThirdParty);
    EXPECT_FALSE(prefs->GetBoolean(prefs::kEnableHyperlinkAuditing));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kEnableReferrers));
    EXPECT_FALSE(prefs->GetBoolean(translate::prefs::kOfferTranslateEnabled));
    EXPECT_EQ(static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled),
              prefs->GetInteger(prefetch::prefs::kNetworkPredictionOptions));
    EXPECT_FALSE(
        prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
    VerifyPrefValueAndControlledState(prefs::kPrivacySandboxM1TopicsEnabled,
                                      base::Value(true),
                                      /* expected_controlled */ false);
    VerifyPrefValueAndControlledState(prefs::kPrivacySandboxM1FledgeEnabled,
                                      base::Value(true),
                                      /* expected_controlled */ false);
    VerifyPrefValueAndControlledState(
        prefs::kPrivacySandboxM1AdMeasurementEnabled, base::Value(true),
        /* expected_controlled */ false);
    VerifyPrefValueAndControlledState(
        prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, base::Value(true),
        /* expected_controlled */ false);
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&std::unique_ptr<ScopedKeepAlive>::reset,
                                  base::Unretained(&keep_alive_), nullptr));
    content::RunAllPendingInMessageLoop();

    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

INSTANTIATE_TEST_SUITE_P(BackgroundPage,
                         ExtensionPreferenceApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionPreferenceApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, Standard) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(embedder_support::kAlternateErrorPagesEnabled, false);
  prefs->SetBoolean(autofill::prefs::kAutofillEnabledDeprecated, false);
  prefs->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, false);
  prefs->SetBoolean(autofill::prefs::kAutofillProfileEnabled, false);
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);
  prefs->SetBoolean(prefs::kEnableHyperlinkAuditing, false);
  prefs->SetBoolean(prefs::kEnableReferrers, false);
  prefs->SetBoolean(translate::prefs::kOfferTranslateEnabled, false);
  prefs->SetInteger(
      prefetch::prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  prefs->SetBoolean(password_manager::prefs::kCredentialsEnableService, false);
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  prefs->SetBoolean(prefs::kSearchSuggestEnabled, false);
  prefs->SetString(prefs::kWebRTCIPHandlingPolicy,
                   blink::kWebRTCIPHandlingDefaultPublicInterfaceOnly);
  prefs->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  prefs->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  prefs->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, true);

  // The 'protectedContentEnabled' pref is only available as browser pref
  // associated with browser profile on ChromeOS and Windows, so pass a JSON
  // array object with any unsupported prefs into the test , so it can skip
  // those.
  static constexpr char kMissingPrefs[] =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
      "[ ]";
#else
      "[ \"protectedContentEnabled\" ]";
#endif

  SetCustomArg(kMissingPrefs);

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("preference/standard");
  {
    extensions::ResultCatcher catcher;
    ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
    EXPECT_TRUE(LoadExtension(extension_path)) << message_;
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    // Run the tests.
    listener.Reply("run test");
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
  CheckPreferencesSet();

  // The settings should not be reset when the extension is reloaded.
  {
    ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
    ReloadExtension(last_loaded_extension_id());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    listener.Reply("");
  }
  CheckPreferencesSet();

  // Uninstalling and installing the extension (without running the test that
  // calls the extension API) should clear the settings.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(profile_), last_loaded_extension_id());
  UninstallExtension(last_loaded_extension_id());
  observer.WaitForExtensionUninstalled();
  CheckPreferencesCleared();

  {
    ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
    EXPECT_TRUE(LoadExtension(extension_path));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    listener.Reply("");
  }
  CheckPreferencesCleared();
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, PersistentIncognito) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(RunExtensionTest("preference/persistent_incognito", {},
                               {.allow_in_incognito = true}))
      << message_;

  // Setting an incognito preference should not create an incognito profile.
  EXPECT_FALSE(profile_->HasPrimaryOTRProfile());

  PrefService* otr_prefs =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)->GetPrefs();
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

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, IncognitoDisabled) {
  EXPECT_FALSE(RunExtensionTest("preference/persistent_incognito"));
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, SessionOnlyIncognito) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(RunExtensionTest("preference/session_only_incognito", {},
                               {.allow_in_incognito = true}))
      << message_;

  EXPECT_TRUE(profile_->HasPrimaryOTRProfile());

  PrefService* otr_prefs =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)->GetPrefs();
  auto* otr_pref = otr_prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(otr_pref);
  EXPECT_TRUE(otr_pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kOff, GetCookieControlsMode(otr_prefs));

  auto* pref = prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kOff, GetCookieControlsMode(prefs));
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, Clear) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);

  EXPECT_TRUE(RunExtensionTest("preference/clear")) << message_;

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kCookieControlsMode);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(CookieControlsMode::kBlockThirdParty, GetCookieControlsMode(prefs));
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, OnChange) {
  EXPECT_TRUE(
      RunExtensionTest("preference/onchange", {}, {.allow_in_incognito = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, OnChangeSplit) {
  extensions::ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile_);
  extensions::ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // Open an incognito window.
  OpenURLOffTheRecord(profile_, GURL("chrome://newtab/"));

  // changeDefault listeners.
  ExtensionTestMessageListener listener1("changeDefault regular ready",
                                         ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito1(
      "changeDefault incognito ready", ReplyBehavior::kWillReply);

  // changeIncognitoOnly listeners.
  ExtensionTestMessageListener listener2("changeIncognitoOnly regular ready",
                                         ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito2(
      "changeIncognitoOnly incognito ready", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener3(
      "changeIncognitoOnly regular listening", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito3(
      "changeIncognitoOnly incognito pref set");

  // changeDefaultOnly listeners.
  ExtensionTestMessageListener listener4("changeDefaultOnly regular ready",
                                         ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito4(
      "changeDefaultOnly incognito ready", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener5("changeDefaultOnly regular pref set");
  ExtensionTestMessageListener listener_incognito5(
      "changeDefaultOnly incognito listening", ReplyBehavior::kWillReply);

  // changeIncognitoOnlyBack listeners.
  ExtensionTestMessageListener listener6(
      "changeIncognitoOnlyBack regular ready", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito6(
      "changeIncognitoOnlyBack incognito ready", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener7(
      "changeIncognitoOnlyBack regular listening", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito7(
      "changeIncognitoOnlyBack incognito pref set");

  // clearIncognito listeners.
  ExtensionTestMessageListener listener8("clearIncognito regular ready",
                                         ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito8(
      "clearIncognito incognito ready", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener9("clearIncognito regular listening",
                                         ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito9(
      "clearIncognito incognito pref cleared");

  // clearDefault listeners.
  ExtensionTestMessageListener listener10("clearDefault regular ready",
                                          ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito10(
      "clearDefault incognito ready", ReplyBehavior::kWillReply);

  base::FilePath extension_data_dir =
      test_data_dir_.AppendASCII("preference").AppendASCII("onchange_split");
  ASSERT_TRUE(LoadExtension(extension_data_dir, {.allow_in_incognito = true}));

  // Test 1 - changeDefault
  EXPECT_TRUE(listener1.WaitUntilSatisfied());            // Regular ready
  EXPECT_TRUE(listener_incognito1.WaitUntilSatisfied());  // Incognito ready
  listener1.Reply("ok");
  listener_incognito1.Reply("ok");

  // Test 2 - changeIncognitoOnly
  EXPECT_TRUE(listener2.WaitUntilSatisfied());            // Regular ready
  EXPECT_TRUE(listener_incognito2.WaitUntilSatisfied());  // Incognito ready
  EXPECT_TRUE(listener3.WaitUntilSatisfied());            // Regular listening
  listener2.Reply("ok");
  listener_incognito2.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito3.WaitUntilSatisfied());
  listener3.Reply("ok");

  // Test 3 - changeDefaultOnly
  EXPECT_TRUE(listener4.WaitUntilSatisfied());            // Regular ready
  EXPECT_TRUE(listener_incognito4.WaitUntilSatisfied());  // Incognito ready
  EXPECT_TRUE(listener_incognito5.WaitUntilSatisfied());  // Incognito listening
  listener4.Reply("ok");
  listener_incognito4.Reply("ok");
  // Regular preference set - notify the incognito listener
  EXPECT_TRUE(listener5.WaitUntilSatisfied());
  listener_incognito5.Reply("ok");

  // Test 4 - changeIncognitoOnlyBack
  EXPECT_TRUE(listener6.WaitUntilSatisfied());            // Regular ready
  EXPECT_TRUE(listener_incognito6.WaitUntilSatisfied());  // Incognito ready
  EXPECT_TRUE(listener7.WaitUntilSatisfied());            // Regular listening
  listener6.Reply("ok");
  listener_incognito6.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito7.WaitUntilSatisfied());
  listener7.Reply("ok");

  // Test 5 - clearIncognito
  EXPECT_TRUE(listener8.WaitUntilSatisfied());            // Regular ready
  EXPECT_TRUE(listener_incognito8.WaitUntilSatisfied());  // Incognito ready
  EXPECT_TRUE(listener9.WaitUntilSatisfied());            // Regular listening
  listener8.Reply("ok");
  listener_incognito8.Reply("ok");
  // Incognito preference cleared -- notify the regular listener
  EXPECT_TRUE(listener_incognito9.WaitUntilSatisfied());
  listener9.Reply("ok");

  // Test 6 - clearDefault
  EXPECT_TRUE(listener10.WaitUntilSatisfied());            // Regular ready
  EXPECT_TRUE(listener_incognito10.WaitUntilSatisfied());  // Incognito ready
  listener10.Reply("ok");
  listener_incognito10.Reply("ok");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher_incognito.message();
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest,
                       OnChangeSplitWithNoOTRProfile) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener loaded_incognito_test_listener(
      "incognito loaded");

  ExtensionTestMessageListener change_pref_listener("change pref value");

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("preference")
                                .AppendASCII("onchange_split_regular_only"),
                            {.allow_in_incognito = true}));

  ASSERT_TRUE(change_pref_listener.WaitUntilSatisfied());
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(loaded_incognito_test_listener.was_satisfied());
  EXPECT_FALSE(profile_->HasPrimaryOTRProfile());
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest,
                       OnChangeSplitWithoutIncognitoAccess) {
  PrefService* prefs = profile_->GetPrefs();
  SetCookieControlsMode(prefs, CookieControlsMode::kBlockThirdParty);

  // Open an incognito window.
  OpenURLOffTheRecord(profile_, GURL("chrome://newtab/"));
  EXPECT_TRUE(profile_->HasPrimaryOTRProfile());

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener loaded_incognito_test_listener(
      "incognito loaded");

  ExtensionTestMessageListener change_pref_listener("change pref value");

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("preference")
                                .AppendASCII("onchange_split_regular_only")));

  ASSERT_TRUE(change_pref_listener.WaitUntilSatisfied());
  SetCookieControlsMode(prefs, CookieControlsMode::kOff);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(loaded_incognito_test_listener.was_satisfied());
}

// Tests the behavior of the Safe Browsing API as described in
// crbug.com/1064722.
IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, SafeBrowsing_SetTrue) {
  ExtensionTestMessageListener listener_true("set to true",
                                             ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_clear("cleared",
                                              ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_false("set to false",
                                              ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_done("done");

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
IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiTest, ThirdPartyCookiesAllowed) {
  ExtensionTestMessageListener listener_true("set to true",
                                             ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_clear("cleared",
                                              ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_false("set to false",
                                              ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_done("done");

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
