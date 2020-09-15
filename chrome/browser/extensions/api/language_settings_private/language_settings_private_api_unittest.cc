// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/fake_input_method_delegate.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace extensions {

typedef api::language_settings_private::SpellcheckDictionaryStatus
    DictionaryStatus;

class MockLanguageSettingsPrivateDelegate
    : public LanguageSettingsPrivateDelegate {
 public:
  explicit MockLanguageSettingsPrivateDelegate(content::BrowserContext* context)
      : LanguageSettingsPrivateDelegate(context) {}
  ~MockLanguageSettingsPrivateDelegate() override {}

  // LanguageSettingsPrivateDelegate:
  std::vector<DictionaryStatus> GetHunspellDictionaryStatuses() override;
  void RetryDownloadHunspellDictionary(const std::string& language) override;

  std::vector<std::string> retry_download_hunspell_dictionary_called_with() {
    return retry_download_hunspell_dictionary_called_with_;
  }

 private:
  std::vector<std::string> retry_download_hunspell_dictionary_called_with_;
};

std::vector<DictionaryStatus>
MockLanguageSettingsPrivateDelegate::GetHunspellDictionaryStatuses() {
  std::vector<DictionaryStatus> statuses;
  DictionaryStatus status;
  status.language_code = "fr";
  status.is_ready = false;
  status.is_downloading = std::make_unique<bool>(true);
  status.download_failed = std::make_unique<bool>(false);
  statuses.push_back(std::move(status));
  return statuses;
}

void MockLanguageSettingsPrivateDelegate::RetryDownloadHunspellDictionary(
    const std::string& language) {
  retry_download_hunspell_dictionary_called_with_.push_back(language);
}

namespace {

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<EventRouter>(profile, ExtensionPrefs::Get(profile));
}

std::unique_ptr<KeyedService> BuildLanguageSettingsPrivateDelegate(
    content::BrowserContext* profile) {
  return std::make_unique<MockLanguageSettingsPrivateDelegate>(profile);
}

std::unique_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return std::make_unique<SpellcheckService>(static_cast<Profile*>(profile));
}

}  // namespace

class LanguageSettingsPrivateApiTest : public ExtensionServiceTestBase {
 public:
  LanguageSettingsPrivateApiTest() = default;
  ~LanguageSettingsPrivateApiTest() override = default;

 protected:
  void RunGetLanguageListTest();

  virtual void InitFeatures() {
#if defined(OS_WIN)
    // Force Windows hybrid spellcheck to be enabled.
    feature_list_.InitAndEnableFeature(spellcheck::kWinUseBrowserSpellChecker);
#endif  // defined(OS_WIN)
  }

#if defined(OS_WIN)
  virtual void AddSpellcheckLanguagesForTesting(
      const std::vector<std::string>& spellcheck_languages_for_testing) {
    SpellcheckServiceFactory::GetInstance()
        ->GetForContext(profile())
        ->InitWindowsDictionaryLanguages(spellcheck_languages_for_testing);
  }

  base::test::ScopedFeatureList feature_list_;
#endif  // defined(OS_WIN)

 private:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceTestBase::InitializeEmptyExtensionService();
    EventRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildEventRouter));

    InitFeatures();

    LanguageSettingsPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildLanguageSettingsPrivateDelegate));

    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&BuildSpellcheckService));
  }

  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(LanguageSettingsPrivateApiTest, RetryDownloadHunspellDictionaryTest) {
  MockLanguageSettingsPrivateDelegate* mock_delegate =
      static_cast<MockLanguageSettingsPrivateDelegate*>(
          LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
              browser_context()));

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateRetryDownloadDictionaryFunction>();

  EXPECT_EQ(
      0u,
      mock_delegate->retry_download_hunspell_dictionary_called_with().size());
  EXPECT_TRUE(
      api_test_utils::RunFunction(function.get(), "[\"fr\"]", profile()))
      << function->GetError();
  EXPECT_EQ(
      1u,
      mock_delegate->retry_download_hunspell_dictionary_called_with().size());
  EXPECT_EQ(
      "fr",
      mock_delegate->retry_download_hunspell_dictionary_called_with().front());
}

TEST_F(LanguageSettingsPrivateApiTest, GetSpellcheckDictionaryStatusesTest) {
  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction>();

  std::unique_ptr<base::Value> actual =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());
  EXPECT_TRUE(actual) << function->GetError();

  base::ListValue expected;
  auto expected_status = std::make_unique<base::DictionaryValue>();
  expected_status->SetString("languageCode", "fr");
  expected_status->SetBoolean("isReady", false);
  expected_status->SetBoolean("isDownloading", true);
  expected_status->SetBoolean("downloadFailed", false);
  expected.Append(std::move(expected_status));
  EXPECT_EQ(expected, *actual);
}

TEST_F(LanguageSettingsPrivateApiTest, GetLanguageListTest) {
  RunGetLanguageListTest();
}

void LanguageSettingsPrivateApiTest::RunGetLanguageListTest() {
  struct LanguageToTest {
    std::string accept_language;
    std::string windows_dictionary_name;  // Empty string indicates to not use
                                          // fake Windows dictionary
    bool is_preferred_language;
    bool is_spellcheck_support_expected;
  };

  std::vector<LanguageToTest> languages_to_test = {
      // Languages with both Windows and Hunspell spellcheck support.
      // GetLanguageList should always report spellchecking to be supported for
      // these languages, regardless of whether a language pack is installed or
      // if it is a preferred language.
      {"fr", "fr-FR", true, true},
      {"de", "de-DE", false, true},
      {"es-MX", "", true, true},
      {"fa", "", false, true},
      {"gl", "", true, false},
      {"zu", "", false, false},
      // Finnish with Filipino language pack (string in string).
      {"fi", "fil", true, false},
      // Sesotho with Asturian language pack (string in string).
      {"st", "ast", true, false},
  };

  // A few more test cases for non-Hunspell languages. These languages do have
  // Windows spellcheck support depending on the OS version. GetLanguageList
  // only reports spellchecking is supported for these languages if the language
  // pack is installed.
#if defined(OS_WIN)
  if (spellcheck::WindowsVersionSupportsSpellchecker()) {
    languages_to_test.push_back({"ar", "ar-SA", true, true});
    languages_to_test.push_back({"bn", "bn-IN", false, true});
  } else {
    languages_to_test.push_back({"ar", "ar-SA", true, false});
    languages_to_test.push_back({"bn", "bn-IN", false, false});
  }
#else
  languages_to_test.push_back({"ar", "ar-SA", true, false});
  languages_to_test.push_back({"bn", "bn-IN", false, false});
#endif  // defined(OS_WIN)

  // Initialize accept languages prefs.
  std::vector<std::string> accept_languages;
  for (auto& language_to_test : languages_to_test) {
    if (language_to_test.is_preferred_language) {
      accept_languages.push_back(language_to_test.accept_language);
    }
  }

  std::string accept_languages_string = base::JoinString(accept_languages, ",");
  DVLOG(2) << "Setting accept languages preferences to: "
           << accept_languages_string;
  profile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                   accept_languages_string);

#if defined(OS_WIN)
  // Add fake Windows dictionaries using InitWindowsDictionaryLanguages.
  std::vector<std::string> windows_spellcheck_languages_for_testing;
  for (auto& language_to_test : languages_to_test) {
    if (!language_to_test.windows_dictionary_name.empty()) {
      windows_spellcheck_languages_for_testing.push_back(
          language_to_test.windows_dictionary_name);
      DVLOG(2) << "Will set fake Windows spellcheck dictionary for testing: "
               << language_to_test.windows_dictionary_name;
    }
  }

  AddSpellcheckLanguagesForTesting(windows_spellcheck_languages_for_testing);
#endif  // defined(OS_WIN)

  auto function =
      base::MakeRefCounted<LanguageSettingsPrivateGetLanguageListFunction>();

  std::unique_ptr<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());

  ASSERT_NE(nullptr, result) << function->GetError();
  EXPECT_TRUE(result->is_list());

  size_t languages_to_test_found_count = 0;
  for (auto& language_val : result->GetList()) {
    EXPECT_TRUE(language_val.is_dict());
    std::string* language_code_ptr = language_val.FindStringKey("code");
    ASSERT_NE(nullptr, language_code_ptr);
    std::string language_code = *language_code_ptr;
    EXPECT_FALSE(language_code.empty());

    const base::Optional<bool> maybe_supports_spellcheck =
        language_val.FindBoolKey("supportsSpellcheck");
    const bool supports_spellcheck = maybe_supports_spellcheck.has_value()
                                         ? maybe_supports_spellcheck.value()
                                         : false;

    for (auto& language_to_test : languages_to_test) {
      if (language_to_test.accept_language == language_code) {
        DVLOG(2) << "*** Found language code being tested=" << language_code
                 << ", supportsSpellcheck=" << supports_spellcheck << " ***";
        EXPECT_EQ(language_to_test.is_spellcheck_support_expected,
                  supports_spellcheck);
        languages_to_test_found_count++;
        break;
      }
    }
  }

  EXPECT_EQ(languages_to_test.size(), languages_to_test_found_count);
}

#if defined(OS_CHROMEOS)
namespace {

namespace input_method = chromeos::input_method;
using input_method::InputMethodDescriptor;
using input_method::InputMethodManager;

std::string GetExtensionImeId() {
  std::string kExtensionImeId = chromeos::extension_ime_util::GetInputMethodID(
      crx_file::id_util::GenerateId("test.extension.ime"), "us");
  return kExtensionImeId;
}

std::string GetComponentExtensionImeId() {
  std::string kComponentExtensionImeId =
      chromeos::extension_ime_util::GetComponentInputMethodID(
          crx_file::id_util::GenerateId("test.component.extension.ime"), "us");
  return kComponentExtensionImeId;
}

std::string GetArcImeId() {
  std::string kArcImeId = chromeos::extension_ime_util::GetArcInputMethodID(
      crx_file::id_util::GenerateId("test.arc.ime"), "us");
  return kArcImeId;
}

class TestInputMethodManager : public input_method::MockInputMethodManager {
 public:
  class TestState : public input_method::MockInputMethodManager::State {
   public:
    TestState() {
      // Set up three IMEs
      std::vector<std::string> layouts({"us"});
      std::vector<std::string> languages({"en-US", "en"});
      std::vector<std::string> arc_languages(
          {chromeos::extension_ime_util::kArcImeLanguage});
      InputMethodDescriptor extension_ime(
          GetExtensionImeId(), "ExtensionIme", "", layouts, languages,
          false /* is_login_keyboard */, GURL(), GURL());
      InputMethodDescriptor component_extension_ime(
          GetComponentExtensionImeId(), "ComponentExtensionIme", "", layouts,
          languages, false /* is_login_keyboard */, GURL(), GURL());
      InputMethodDescriptor arc_ime(
          GetArcImeId(), "ArcIme", "", layouts, arc_languages,
          false /* is_login_keyboard */, GURL(), GURL());
      input_methods_ = {extension_ime, component_extension_ime, arc_ime};
    }

    void GetInputMethodExtensions(
        input_method::InputMethodDescriptors* descriptors) override {
      for (const auto& descriptor : input_methods_)
        descriptors->push_back(descriptor);
    }

    input_method::InputMethodDescriptors input_methods_;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override = default;

    DISALLOW_COPY_AND_ASSIGN(TestState);
  };

  TestInputMethodManager() : state_(new TestState), util_(&delegate_) {
    util_.AppendInputMethods(state_->input_methods_);
    mock_delegate_ =
        new chromeos::input_method::MockComponentExtIMEManagerDelegate();
    component_ext_mgr_ =
        std::make_unique<chromeos::ComponentExtensionIMEManager>();
    component_ext_mgr_->Initialize(base::WrapUnique(mock_delegate_));
  }

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  input_method::InputMethodUtil* GetInputMethodUtil() override {
    return &util_;
  }

  chromeos::ComponentExtensionIMEManager* GetComponentExtensionIMEManager()
      override {
    return component_ext_mgr_.get();
  }

 private:
  scoped_refptr<TestState> state_;
  input_method::FakeInputMethodDelegate delegate_;
  input_method::InputMethodUtil util_;
  std::unique_ptr<chromeos::ComponentExtensionIMEManager> component_ext_mgr_;
  chromeos::input_method::MockComponentExtIMEManagerDelegate* mock_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManager);
};

}  // namespace

TEST_F(LanguageSettingsPrivateApiTest, GetInputMethodListsTest) {
  TestInputMethodManager::Initialize(new TestInputMethodManager);

  // Initialize relevant prefs.
  StringPrefMember enabled_imes;
  enabled_imes.Init(prefs::kLanguageEnabledImes, profile()->GetPrefs());
  StringPrefMember preload_engines;
  preload_engines.Init(prefs::kLanguagePreloadEngines, profile()->GetPrefs());

  enabled_imes.SetValue(
      base::JoinString({GetExtensionImeId(), GetArcImeId()}, ","));
  preload_engines.SetValue(GetComponentExtensionImeId());

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateGetInputMethodListsFunction>();
  std::unique_ptr<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());

  ASSERT_NE(nullptr, result) << function->GetError();
  EXPECT_TRUE(result->is_dict());

  base::Value* input_methods = result->FindListKey("thirdPartyExtensionImes");
  ASSERT_NE(input_methods, nullptr);
  EXPECT_EQ(3u, input_methods->GetList().size());

  for (auto& input_method : input_methods->GetList()) {
    base::Value* ime_tags_ptr = input_method.FindListKey("tags");
    ASSERT_NE(nullptr, ime_tags_ptr);

    // Check tags contain input method's display name
    base::Value* ime_name_ptr = input_method.FindKey("displayName");
    EXPECT_TRUE(base::Contains(ime_tags_ptr->GetList(), *ime_name_ptr));

    // Check tags contain input method's language codes' display names
    base::Value* ime_language_codes_ptr =
        input_method.FindListKey("languageCodes");
    ASSERT_NE(nullptr, ime_language_codes_ptr);
    for (auto& language_code : ime_language_codes_ptr->GetList()) {
      std::string language_display_name =
          base::UTF16ToUTF8(l10n_util::GetDisplayNameForLocale(
              language_code.GetString(), "en", true));
      if (!language_display_name.empty())
        EXPECT_TRUE(base::Contains(ime_tags_ptr->GetList(),
                                   base::Value(language_display_name)));
    }
  }

  TestInputMethodManager::Shutdown();
}

TEST_F(LanguageSettingsPrivateApiTest, AddInputMethodTest) {
  TestInputMethodManager::Initialize(new TestInputMethodManager);

  // Initialize relevant prefs.
  profile()->GetPrefs()->SetString(language::prefs::kPreferredLanguages,
                                   "en-US");
  StringPrefMember enabled_imes;
  enabled_imes.Init(prefs::kLanguageEnabledImes, profile()->GetPrefs());
  StringPrefMember preload_engines;
  preload_engines.Init(prefs::kLanguagePreloadEngines, profile()->GetPrefs());
  enabled_imes.SetValue(std::string());
  preload_engines.SetValue(std::string());

  {
    // Add an extension IME. kLanguageEnabledImes should be updated.
    auto function =
        base::MakeRefCounted<LanguageSettingsPrivateAddInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetExtensionImeId() + "\"]", profile());

    EXPECT_EQ(GetExtensionImeId(), enabled_imes.GetValue());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  enabled_imes.SetValue(std::string());
  preload_engines.SetValue(std::string());
  {
    // Add a component extension IME. kLanguagePreloadEngines should be
    // updated.
    auto function =
        base::MakeRefCounted<LanguageSettingsPrivateAddInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetComponentExtensionImeId() + "\"]",
        profile());

    EXPECT_TRUE(enabled_imes.GetValue().empty());
    EXPECT_EQ(GetComponentExtensionImeId(), preload_engines.GetValue());
  }

  enabled_imes.SetValue(std::string());
  preload_engines.SetValue(std::string());
  {
    // Add an ARC IME. kLanguageEnabledImes should be updated.
    auto function =
        base::MakeRefCounted<LanguageSettingsPrivateAddInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetArcImeId() + "\"]", profile());

    EXPECT_EQ(GetArcImeId(), enabled_imes.GetValue());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  TestInputMethodManager::Shutdown();
}

TEST_F(LanguageSettingsPrivateApiTest, RemoveInputMethodTest) {
  TestInputMethodManager::Initialize(new TestInputMethodManager);

  // Initialize relevant prefs.
  StringPrefMember enabled_imes;
  enabled_imes.Init(prefs::kLanguageEnabledImes, profile()->GetPrefs());
  StringPrefMember preload_engines;
  preload_engines.Init(prefs::kLanguagePreloadEngines, profile()->GetPrefs());

  enabled_imes.SetValue(
      base::JoinString({GetExtensionImeId(), GetArcImeId()}, ","));
  preload_engines.SetValue(GetComponentExtensionImeId());
  {
    // Remove an extension IME.
    auto function = base::MakeRefCounted<
        LanguageSettingsPrivateRemoveInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetExtensionImeId() + "\"]", profile());

    EXPECT_EQ(GetArcImeId(), enabled_imes.GetValue());
    EXPECT_EQ(GetComponentExtensionImeId(), preload_engines.GetValue());
  }

  {
    // Remove a component extension IME.
    auto function = base::MakeRefCounted<
        LanguageSettingsPrivateRemoveInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetComponentExtensionImeId() + "\"]",
        profile());

    EXPECT_EQ(GetArcImeId(), enabled_imes.GetValue());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  {
    // Remove an ARC IME.
    auto function = base::MakeRefCounted<
        LanguageSettingsPrivateRemoveInputMethodFunction>();
    api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + GetArcImeId() + "\"]", profile());

    EXPECT_TRUE(enabled_imes.GetValue().empty());
    EXPECT_TRUE(preload_engines.GetValue().empty());
  }

  TestInputMethodManager::Shutdown();
}

#endif  // OS_CHROMEOS

#if defined(OS_WIN)
class LanguageSettingsPrivateApiTestDelayInit
    : public LanguageSettingsPrivateApiTest {
 public:
  LanguageSettingsPrivateApiTestDelayInit() = default;

 protected:
  void InitFeatures() override {
    // Force Windows hybrid spellcheck and delayed initialization of the
    // spellcheck service to be enabled.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{spellcheck::kWinUseBrowserSpellChecker,
                              spellcheck::kWinDelaySpellcheckServiceInit},
        /*disabled_features=*/{});
  }

  void AddSpellcheckLanguagesForTesting(
      const std::vector<std::string>& spellcheck_languages_for_testing)
      override {
    SpellcheckServiceFactory::GetInstance()
        ->GetForContext(profile())
        ->AddSpellcheckLanguagesForTesting(spellcheck_languages_for_testing);
  }
};

TEST_F(LanguageSettingsPrivateApiTestDelayInit, GetLanguageListTest) {
  RunGetLanguageListTest();
}
#endif  // defined(OS_WIN)

}  // namespace extensions
