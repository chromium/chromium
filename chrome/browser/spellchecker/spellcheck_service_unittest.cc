// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <optional>
#include <ostream>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestCase {
  TestCase(
      const std::string& accept_languages,
      const std::vector<std::string>& spellcheck_dictionaries,
      const std::vector<std::string>& expected_languages,
      const std::vector<std::string>& expected_languages_used_for_spellcheck)
      : accept_languages(accept_languages),
        spellcheck_dictionaries(spellcheck_dictionaries) {
    SpellcheckService::Dictionary dictionary;
    for (const auto& language : expected_languages) {
      if (!language.empty()) {
        dictionary.language = language;
        dictionary.used_for_spellcheck =
            base::Contains(expected_languages_used_for_spellcheck, language);
        expected_dictionaries.push_back(dictionary);
      }
    }
  }

  ~TestCase() {}

  std::string accept_languages;
  std::vector<std::string> spellcheck_dictionaries;
  std::vector<SpellcheckService::Dictionary> expected_dictionaries;
};

bool operator==(const SpellcheckService::Dictionary& lhs,
                const SpellcheckService::Dictionary& rhs) {
  return lhs.language == rhs.language &&
         lhs.used_for_spellcheck == rhs.used_for_spellcheck;
}

std::ostream& operator<<(std::ostream& out,
                         const SpellcheckService::Dictionary& dictionary) {
  out << "{\"" << dictionary.language << "\", used_for_spellcheck="
      << (dictionary.used_for_spellcheck ? "true " : "false") << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << "language::prefs::kAcceptLanguages=[" << test_case.accept_languages
      << "], prefs::kSpellCheckDictionaries=["
      << base::JoinString(test_case.spellcheck_dictionaries, ",")
      << "], expected=[";
  for (const auto& dictionary : test_case.expected_dictionaries) {
    out << dictionary << ",";
  }
  out << "]";
  return out;
}

static std::unique_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return std::make_unique<SpellcheckService>(static_cast<Profile*>(profile));
}

class SpellcheckServiceUnitTestBase : public testing::Test {
 public:
  SpellcheckServiceUnitTestBase() = default;

  SpellcheckServiceUnitTestBase(const SpellcheckServiceUnitTestBase&) = delete;
  SpellcheckServiceUnitTestBase& operator=(
      const SpellcheckServiceUnitTestBase&) = delete;

  ~SpellcheckServiceUnitTestBase() override = default;

  content::BrowserContext* browser_context() { return &profile_; }
  PrefService* prefs() { return profile_.GetPrefs(); }

 protected:
  void SetUp() override {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, base::BindRepeating(&BuildSpellcheckService));
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
};

class SpellcheckServiceUnitTest : public SpellcheckServiceUnitTestBase,
                                  public testing::WithParamInterface<TestCase> {
 private:
#if BUILDFLAG(IS_WIN)
  // Tests were designed assuming Hunspell dictionary used and may fail when
  // Windows spellcheck is enabled by default.
  spellcheck::ScopedDisableBrowserSpellCheckerForTesting
      disable_browser_spell_checker_;
#endif  // BUILDFLAG(IS_WIN)
};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckServiceUnitTest,
    testing::Values(
        TestCase("en-JP,aa", {"aa"}, {}, {}),
        TestCase("en,aa", {"aa"}, {"en"}, {}),
        TestCase("en,en-JP,fr,aa", {"fr"}, {"en", "fr"}, {"fr"}),
        TestCase("en,en-JP,fr,zz,en-US", {"fr"}, {"en", "fr", "en-US"}, {"fr"}),
        TestCase("en,en-US,en-GB",
                 {"en-GB"},
                 {"en", "en-US", "en-GB"},
                 {"en-GB"}),
        TestCase("en,en-US,en-AU",
                 {"en-AU"},
                 {"en", "en-US", "en-AU"},
                 {"en-AU"}),
        TestCase("en,en-US,en-AU",
                 {"en-US"},
                 {"en", "en-US", "en-AU"},
                 {"en-US"}),
        TestCase("en,en-US", {"en-US"}, {"en", "en-US"}, {"en-US"}),
        TestCase("en,en-US,fr", {"en-US"}, {"en", "en-US", "fr"}, {"en-US"}),
        TestCase("en,fr,en-US,en-AU",
                 {"en-US", "fr"},
                 {"en", "fr", "en-US", "en-AU"},
                 {"fr", "en-US"}),
        TestCase("en-US,en", {"en-US"}, {"en-US", "en"}, {"en-US"}),
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
        // Scenario where user disabled the Windows spellcheck feature with some
        // non-Hunspell languages set in preferences.
        TestCase("fr,eu,en-US,ar",
                 {"fr", "eu", "en-US", "ar"},
                 {"fr", "en-US"},
                 {"fr", "en-US"}),
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
        TestCase("hu-HU,hr-HR", {"hr"}, {"hu", "hr"}, {"hr"})));

TEST_P(SpellcheckServiceUnitTest, GetDictionaries) {
  prefs()->SetString(language::prefs::kAcceptLanguages,
                     GetParam().accept_languages);
  base::Value::List spellcheck_dictionaries;
  for (const std::string& dictionary : GetParam().spellcheck_dictionaries) {
    spellcheck_dictionaries.Append(dictionary);
  }
  prefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                   std::move(spellcheck_dictionaries));

  std::vector<SpellcheckService::Dictionary> dictionaries;
  SpellcheckService::GetDictionaries(browser_context(), &dictionaries);

  EXPECT_EQ(GetParam().expected_dictionaries, dictionaries);
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
class SpellcheckServiceHybridUnitTestBase
    : public SpellcheckServiceUnitTestBase {
 public:
  SpellcheckServiceHybridUnitTestBase() = default;

 protected:
  void SetUp() override {
    InitFeatures();

    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, base::BindRepeating(&BuildSpellcheckService));
  }

  virtual void InitFeatures() {}

  virtual void InitializeSpellcheckService(
      const std::vector<std::string>& spellcheck_languages_for_testing) {
    // Fake the presence of Windows spellcheck dictionaries.
    spellcheck_service_ =
        SpellcheckServiceFactory::GetInstance()->GetForContext(
            browser_context());

    spellcheck_service_->InitWindowsDictionaryLanguages(
        spellcheck_languages_for_testing);

    ASSERT_TRUE(spellcheck_service_->dictionaries_loaded());
  }

  void RunGetDictionariesTest(
      const std::string accept_languages,
      const std::vector<std::string> spellcheck_dictionaries,
      const std::vector<SpellcheckService::Dictionary> expected_dictionaries);

  void RunDictionaryMappingTest(
      const std::string full_tag,
      const std::string expected_accept_language,
      const std::string expected_tag_passed_to_spellcheck,
      const std::string expected_accept_language_generic,
      const std::string expected_tag_passed_to_spellcheck_generic);

  // Used for faking the presence of Windows spellcheck dictionaries.
  static const std::vector<std::string>
      windows_spellcheck_languages_for_testing_;

  base::test::ScopedFeatureList feature_list_;

  raw_ptr<SpellcheckService> spellcheck_service_;
};

void SpellcheckServiceHybridUnitTestBase::RunGetDictionariesTest(
    const std::string accept_languages,
    const std::vector<std::string> spellcheck_dictionaries,
    const std::vector<SpellcheckService::Dictionary> expected_dictionaries) {
  prefs()->SetString(language::prefs::kAcceptLanguages, accept_languages);
  base::Value::List spellcheck_dictionaries_list;
  for (std::string dict : spellcheck_dictionaries) {
    spellcheck_dictionaries_list.Append(dict);
  }
  prefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                   std::move(spellcheck_dictionaries_list));

  // Simulate first-run scenario (method is normally called during browser
  // start-up). If the primary accept language has no dictionary support, it is
  // expected that spellchecking will be disabled for that language.
  SpellcheckService::EnableFirstUserLanguageForSpellcheck(prefs());

  InitializeSpellcheckService(windows_spellcheck_languages_for_testing_);

  std::vector<SpellcheckService::Dictionary> dictionaries;
  SpellcheckService::GetDictionaries(browser_context(), &dictionaries);

  EXPECT_EQ(expected_dictionaries, dictionaries);
}

void SpellcheckServiceHybridUnitTestBase::RunDictionaryMappingTest(
    const std::string full_tag,
    const std::string expected_accept_language,
    const std::string expected_tag_passed_to_spellcheck,
    const std::string expected_accept_language_generic,
    const std::string expected_tag_passed_to_spellcheck_generic) {
  InitializeSpellcheckService({full_tag});

  std::string supported_dictionary;
  if (!expected_accept_language.empty()) {
    supported_dictionary =
        spellcheck_service_->GetSupportedWindowsDictionaryLanguage(
            expected_accept_language);
    EXPECT_FALSE(supported_dictionary.empty());
    EXPECT_EQ(full_tag, supported_dictionary);
    EXPECT_EQ(expected_tag_passed_to_spellcheck,
              SpellcheckService::GetTagToPassToWindowsSpellchecker(
                  expected_accept_language, full_tag));

    // Special case for Serbian. The "sr" accept language is interpreted as
    // using Cyrillic script. There should be an extra entry in the windows
    // dictionary map if Cyrillic windows dictionary is installed.
    if (base::EqualsCaseInsensitiveASCII(
            "sr-Cyrl", SpellcheckService::GetLanguageAndScriptTag(
                           full_tag,
                           /* include_script_tag= */ true))) {
      EXPECT_EQ(
          full_tag,
          spellcheck_service_->GetSupportedWindowsDictionaryLanguage("sr"));
    } else {
      EXPECT_TRUE(
          spellcheck_service_->GetSupportedWindowsDictionaryLanguage("sr")
              .empty());
    }

    if (!expected_accept_language_generic.empty()) {
      supported_dictionary =
          spellcheck_service_->GetSupportedWindowsDictionaryLanguage(
              expected_accept_language_generic);
      EXPECT_FALSE(supported_dictionary.empty());
      EXPECT_EQ(expected_accept_language_generic, supported_dictionary);
      EXPECT_EQ(expected_tag_passed_to_spellcheck_generic,
                SpellcheckService::GetTagToPassToWindowsSpellchecker(
                    expected_accept_language_generic, supported_dictionary));
    } else {
      // Should only be one entry in the map.
      EXPECT_EQ(1u,
                spellcheck_service_->windows_spellcheck_dictionary_map_.size());
    }
  } else {
    // Unsupported language--should not be in map.
    EXPECT_TRUE(
        spellcheck_service_->windows_spellcheck_dictionary_map_.empty());
  }
}

// static
const std::vector<std::string> SpellcheckServiceHybridUnitTestBase::
    windows_spellcheck_languages_for_testing_ = {
        "fr-FR",   // Has both Windows and Hunspell support.
        "es-MX",   // Has both Windows and Hunspell support, but for Hunspell
                   // maps to es-ES.
        "gl-ES",   // (Galician) Has only Windows support, no Hunspell
                   // dictionary.
        "fi-FI",   // (Finnish) Has only Windows support, no Hunspell
                   // dictionary.
        "it-IT",   // Has both Windows and Hunspell support.
        "pt-BR",   // Has both Windows and Hunspell support.
        "haw-US",  // (Hawaiian) No Hunspell dictionary. Note that first two
                   // letters of language code are "ha," the same as Hausa.
        "ast",     // (Asturian) Has only Windows support, no Hunspell
                   // dictionary. Note that last two letters of language
                   // code are "st," the same as Sesotho.
        "kok-Deva-IN",       // Konkani (Devanagari, India)--note presence of
                             // script subtag.
        "sr-Cyrl-ME",        // Serbian (Cyrillic, Montenegro)--note presence of
                             // script subtag.
        "sr-Latn-ME",        // Serbian (Latin, Montenegro)--note presence of
                             // script subtag.
        "ja-Latn-JP-x-ext",  // Japanese with Latin script--note presence of
                             // private use subtag. Ignore private use
                             // dictionaries.
};

class GetDictionariesHybridUnitTestNoDelayInit
    : public SpellcheckServiceHybridUnitTestBase,
      public testing::WithParamInterface<TestCase> {
 protected:
  void InitFeatures() override {
    // Disable kWinDelaySpellcheckServiceInit, as the case where it's enabled
    // is tested in SpellcheckServiceWindowsDictionaryMappingUnitTestDelayInit.
    feature_list_.InitAndDisableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);
  }
};

static const TestCase kHybridGetDictionariesParams[] = {
    // Galician (gl) has only Windows support, no Hunspell dictionary. Croatian
    // (hr) has only Hunspell support, no local Windows dictionary. First
    // language is supported by windows and should be spellchecked.
    TestCase("gl", {}, {"gl"}, {"gl"}),
    TestCase("gl", {"gl"}, {"gl"}, {"gl"}),
    TestCase("gl,hr", {}, {"gl", "hr"}, {"gl"}),
    TestCase("gl,hr", {"gl"}, {"gl", "hr"}, {"gl"}),
    TestCase("gl,hr", {"hr"}, {"gl", "hr"}, {"gl", "hr"}),
    TestCase("gl,hr", {"gl", "hr"}, {"gl", "hr"}, {"gl", "hr"}),
    TestCase("hr", {}, {"hr"}, {"hr"}),
    TestCase("hr", {"hr"}, {"hr"}, {"hr"}),
    TestCase("hr,gl", {"hr"}, {"hr", "gl"}, {"hr"}),
    // Cebuano (ceb) is a language with neither Windows or Hunspell support,
    // should be unset if was enabled during simulated "first run" scenario.
    TestCase("ceb", {}, {}, {}),
    TestCase("ceb,gl,hr", {"gl", "hr"}, {"gl", "hr"}, {"gl", "hr"}),
    // Finnish has only "fi" in hard-coded list of accept languages.
    TestCase("fi-FI,fi,en-US,en",
             {"en-US"},
             {"fi", "en-US", "en"},
             {"fi", "en-US"}),
    // First language is supported by Windows but private use dictionaries
    // are ignored.
    TestCase("ja,gl", {"gl"}, {"gl"}, {"gl"}),
    // (Basque) No Hunspell support, has Windows support but
    // language pack not present.
    TestCase("eu", {"eu"}, {}, {}),
    TestCase("es-419,es-MX",
             {"es-419", "es-MX"},
             {"es-419", "es-MX"},
             {"es-419", "es-MX"}),
    TestCase("fr-FR,es-MX,gl,pt-BR,hr,it",
             {"fr-FR", "gl", "pt-BR", "it"},
             {"fr-FR", "es-MX", "gl", "pt-BR", "hr", "it"},
             {"fr-FR", "gl", "pt-BR", "it"}),
    // Hausa with Hawaiian language pack (ha/haw string in string).
    TestCase("ha", {"ha"}, {}, {}),
    // Sesotho with Asturian language pack (st/ast string in string).
    TestCase("st", {"st"}, {}, {}),
    // User chose generic Serbian in languages preferences (which uses
    // Cyrillic script).
    TestCase("sr,sr-Latn-RS", {"sr", "sr-Latn-RS"}, {"sr"}, {"sr"}),
    // If there is platform spellcheck support for a regional variation of
    // a language, the generic version should also be toggleable in spellcheck
    // settings. There is no Hunspell dictionary for generic Portuguese (pt);
    // there is Hunspell support for generic Italian (it) but the platform
    // dictionary should be used instead.
    TestCase("pt,pt-BR", {"pt", "pt-BR"}, {"pt", "pt-BR"}, {"pt", "pt-BR"}),
    TestCase("it,it-IT", {"it", "it-IT"}, {"it", "it-IT"}, {"it", "it-IT"}),
};

INSTANTIATE_TEST_SUITE_P(TestCases,
                         GetDictionariesHybridUnitTestNoDelayInit,
                         testing::ValuesIn(kHybridGetDictionariesParams));

TEST_P(GetDictionariesHybridUnitTestNoDelayInit, GetDictionaries) {
  RunGetDictionariesTest(GetParam().accept_languages,
                         GetParam().spellcheck_dictionaries,
                         GetParam().expected_dictionaries);
}

struct DictionaryMappingTestCase {
  std::string full_tag;
  std::string expected_accept_language;
  std::string expected_tag_passed_to_spellcheck;
  std::string expected_accept_language_generic;
  std::string expected_tag_passed_to_spellcheck_generic;
};

std::ostream& operator<<(std::ostream& out,
                         const DictionaryMappingTestCase& test_case) {
  out << "full_tag=" << test_case.full_tag
      << ", expected_accept_language=" << test_case.expected_accept_language
      << ", expected_tag_passed_to_spellcheck="
      << test_case.expected_tag_passed_to_spellcheck
      << ", expected_accept_language_generic="
      << test_case.expected_accept_language_generic
      << ", expected_tag_passed_to_spellcheck_generic="
      << test_case.expected_tag_passed_to_spellcheck_generic;

  return out;
}

class SpellcheckServiceWindowsDictionaryMappingUnitTest
    : public SpellcheckServiceHybridUnitTestBase,
      public testing::WithParamInterface<DictionaryMappingTestCase> {
 protected:
  void InitFeatures() override {
    // Disable kWinDelaySpellcheckServiceInit, as the case where it's enabled
    // is tested in SpellcheckServiceWindowsDictionaryMappingUnitTestDelayInit.
    feature_list_.InitAndDisableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);
  }
};

static const DictionaryMappingTestCase kHybridDictionaryMappingsParams[] = {
    DictionaryMappingTestCase({"en-CA", "en-CA", "en-CA", "en", "en"}),
    DictionaryMappingTestCase({"en-PH", "en", "en", "", ""}),
    DictionaryMappingTestCase({"es-MX", "es-MX", "es-MX", "es", "es"}),
    DictionaryMappingTestCase({"ar-SA", "ar", "ar", "", ""}),
    DictionaryMappingTestCase({"kok-Deva-IN", "kok", "kok-Deva", "", ""}),
    DictionaryMappingTestCase({"sr-Cyrl-RS", "sr", "sr-Cyrl", "", ""}),
    DictionaryMappingTestCase({"sr-Cyrl-ME", "sr", "sr-Cyrl", "", ""}),
    // Only sr with Cyrillic implied supported in Chromium.
    DictionaryMappingTestCase({"sr-Latn-RS", "", "sr-Latn", "", ""}),
    DictionaryMappingTestCase({"sr-Latn-ME", "", "sr-Latn", "", ""}),
    DictionaryMappingTestCase({"ca-ES", "ca", "ca", "", ""}),
    DictionaryMappingTestCase({"ca-ES-valencia", "ca", "ca", "", ""}),
    // If there is platform spellcheck support for a regional variation of
    // a language, the generic version should also be toggleable in spellcheck
    // settings. There is no Hunspell dictionary for generic Portuguese (pt);
    // there is Hunspell support for generic Italian (it) but the platform
    // dictionary should be used instead.
    DictionaryMappingTestCase({"it-IT", "it-IT", "it-IT", "it", "it"}),
    DictionaryMappingTestCase({"pt-BR", "pt-BR", "pt-BR", "pt", "pt"}),
};

INSTANTIATE_TEST_SUITE_P(TestCases,
                         SpellcheckServiceWindowsDictionaryMappingUnitTest,
                         testing::ValuesIn(kHybridDictionaryMappingsParams));

TEST_P(SpellcheckServiceWindowsDictionaryMappingUnitTest, CheckMappings) {
  RunDictionaryMappingTest(
      GetParam().full_tag, GetParam().expected_accept_language,
      GetParam().expected_tag_passed_to_spellcheck,
      GetParam().expected_accept_language_generic,
      GetParam().expected_tag_passed_to_spellcheck_generic);
}

class SpellcheckServiceHybridUnitTestDelayInitBase
    : public SpellcheckServiceHybridUnitTestBase {
 public:
  SpellcheckServiceHybridUnitTestDelayInitBase() = default;

  void OnDictionariesInitialized() {
    dictionaries_initialized_received_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

 protected:
  void InitFeatures() override {
    // Don't initialize the SpellcheckService on browser launch.
    feature_list_.InitAndEnableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);
  }

  void InitializeSpellcheckService(
      const std::vector<std::string>& spellcheck_languages_for_testing)
      override {
    // Fake the presence of Windows spellcheck dictionaries.
    spellcheck_service_ =
        SpellcheckServiceFactory::GetInstance()->GetForContext(
            browser_context());

    spellcheck_service_->AddSpellcheckLanguagesForTesting(
        spellcheck_languages_for_testing);

    // Asynchronously load the dictionaries.
    ASSERT_FALSE(spellcheck_service_->dictionaries_loaded());
    spellcheck_service_->InitializeDictionaries(
        base::BindOnce(&SpellcheckServiceHybridUnitTestDelayInitBase::
                           OnDictionariesInitialized,
                       base::Unretained(this)));

    RunUntilCallbackReceived();
    ASSERT_TRUE(spellcheck_service_->dictionaries_loaded());
  }

  void RunUntilCallbackReceived() {
    if (dictionaries_initialized_received_)
      return;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    // reset status.
    dictionaries_initialized_received_ = false;
  }

 private:
  bool dictionaries_initialized_received_ = false;

  // Quits the RunLoop on receiving the callback from InitializeDictionaries.
  base::OnceClosure quit_;
};

class SpellcheckServiceHybridUnitTestDelayInit
    : public SpellcheckServiceHybridUnitTestDelayInitBase,
      public testing::WithParamInterface<TestCase> {};

INSTANTIATE_TEST_SUITE_P(TestCases,
                         SpellcheckServiceHybridUnitTestDelayInit,
                         testing::ValuesIn(kHybridGetDictionariesParams));

TEST_P(SpellcheckServiceHybridUnitTestDelayInit, GetDictionaries) {
  RunGetDictionariesTest(GetParam().accept_languages,
                         GetParam().spellcheck_dictionaries,
                         GetParam().expected_dictionaries);
}

class SpellcheckServiceWindowsDictionaryMappingUnitTestDelayInit
    : public SpellcheckServiceHybridUnitTestDelayInitBase,
      public testing::WithParamInterface<DictionaryMappingTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckServiceWindowsDictionaryMappingUnitTestDelayInit,
    testing::ValuesIn(kHybridDictionaryMappingsParams));

TEST_P(SpellcheckServiceWindowsDictionaryMappingUnitTestDelayInit,
       CheckMappings) {
  RunDictionaryMappingTest(
      GetParam().full_tag, GetParam().expected_accept_language,
      GetParam().expected_tag_passed_to_spellcheck,
      GetParam().expected_accept_language_generic,
      GetParam().expected_tag_passed_to_spellcheck_generic);
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
