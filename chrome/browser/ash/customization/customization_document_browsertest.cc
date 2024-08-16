// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/customization/customization_document.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using locale_util::LanguageSwitchResult;
using locale_util::SwitchLanguageCallback;

class LanguageSwitchedWaiter {
 public:
  explicit LanguageSwitchedWaiter(SwitchLanguageCallback callback)
      : callback_(std::move(callback)),
        finished_(false),
        runner_(new content::MessageLoopRunner) {}

  LanguageSwitchedWaiter(const LanguageSwitchedWaiter&) = delete;
  LanguageSwitchedWaiter& operator=(const LanguageSwitchedWaiter&) = delete;

  void ExitMessageLoop(const LanguageSwitchResult& result) {
    finished_ = true;
    runner_->Quit();
    std::move(callback_).Run(result);
  }

  void Wait() {
    if (finished_)
      return;
    runner_->Run();
  }

  SwitchLanguageCallback Callback() {
    return SwitchLanguageCallback(base::BindOnce(
        &LanguageSwitchedWaiter::ExitMessageLoop, base::Unretained(this)));
  }

 private:
  SwitchLanguageCallback callback_;
  bool finished_;
  scoped_refptr<content::MessageLoopRunner> runner_;
};

const struct {
  const char* locale_alias;
  const char* locale_name;
} locale_aliases[] = {{"en-AU", "en-GB"},
                      {"en-CA", "en-GB"},
                      {"en-NZ", "en-GB"},
                      {"en-ZA", "en-GB"},
                      {"fr-CA", "fr"},
                      {"no", "nb"},
                      {"iw", "he"}};

// Several language IDs are actually aliases to another IDs, so real language
// ID is reported as "loaded" when alias is requested.
std::string GetExpectedLanguage(const std::string& required) {
  std::string expected = required;

  for (size_t i = 0; i < std::size(locale_aliases); ++i) {
    if (required != locale_aliases[i].locale_alias)
      continue;

    expected = locale_aliases[i].locale_name;
    break;
  }

  return expected;
}

void VerifyLanguageSwitched(const LanguageSwitchResult& result) {
  EXPECT_TRUE(result.success) << "SwitchLanguage failed: required='"
                              << result.requested_locale << "', actual='"
                              << result.loaded_locale
                              << "', success=" << result.success;
  EXPECT_EQ(GetExpectedLanguage(result.requested_locale), result.loaded_locale)
      << "SwitchLanguage failed: required='" << result.requested_locale
      << "', actual='" << result.loaded_locale
      << "', success=" << result.success;
}

std::string Print(const std::vector<std::string>& locales) {
  std::string result("{");
  for (size_t i = 0; i < locales.size(); ++i) {
    if (i != 0) {
      result += ", ";
    }
    result += "'";
    result += locales[i];
    result += "'";
  }
  result += "}";
  return result;
}

const char* kVPDInitialLocales[] = {
    "ar",
    "ar,bg",
    "ar,bg,bn",
    "ar,bg,bn,ca",
    "ar,bg,bn,ca,cs,da,de,el,en-AU,en-CA,en-GB,en-NZ,en-US,en-ZA,es,es-419,et,"
    "fa,fi,fil,fr,fr-CA,gu,he,hi,hr,hu,id,it,ja,kn,ko,lt,lv,ml,mr,ms,nl,nb,pl,"
    "pt-BR,pt-PT,ro,ru,sk,sl,sr,sv,ta,te,th,tr,vi,zh-CN,zh-TW",
};

const std::vector<std::string> languages_available = {
    "ar",
    "bg",
    "bn",
    "ca",
    "cs",
    "da",
    "de",
    "el",
    "en-AU",
    "en-CA",
    "en-GB",
    "en-NZ",
    "en-US",
    "en-ZA",
    "es",
    "es-419",
    "et",
    "fa",
    "fi",
    "fil",
    "fr",
    "fr-CA",
    "gu",
    "he",
    "hi",
    "hr",
    "hu",
    "id",
    "it",
    "ja",
    "kn",
    "ko",
    "lt",
    "lv",
    "ml",
    "mr",
    "ms",
    "nl",
    "nb",
    "pl",
    "pt-BR",
    "pt-PT",
    "ro",
    "ru",
    "sk",
    "sl",
    "sr",
    "sv",
    "ta",
    "te",
    "th",
    "tr",
    "vi",
    "zh-CN",
    "zh-TW"
};

}  // anonymous namespace

typedef InProcessBrowserTest CustomizationLocaleTest;

IN_PROC_BROWSER_TEST_F(CustomizationLocaleTest, CheckAvailableLocales) {
  for (size_t i = 0; i < languages_available.size(); ++i) {
    LanguageSwitchedWaiter waiter(base::BindOnce(&VerifyLanguageSwitched));
    locale_util::SwitchLanguage(languages_available[i], true, true,
                                waiter.Callback(),
                                ProfileManager::GetActiveUserProfile());
    waiter.Wait();
    {
      std::string resolved_locale;
      base::ScopedAllowBlockingForTesting allow_blocking;
      l10n_util::CheckAndResolveLocale(languages_available[i],
                                       &resolved_locale);
      EXPECT_EQ(GetExpectedLanguage(languages_available[i]), resolved_locale)
          << "CheckAndResolveLocale() failed for language='"
          << languages_available[i] << "'";
    }
  }
}

class CustomizationVPDTest : public InProcessBrowserTest,
                             public testing::WithParamInterface<const char*> {
 public:
  CustomizationVPDTest()
      : statistics_provider_(new system::FakeStatisticsProvider()) {
    // Set the instance returned by GetInstance() for testing.
    system::StatisticsProvider::SetTestProvider(statistics_provider_.get());
    statistics_provider_->SetMachineStatistic("initial_locale", GetParam());
    statistics_provider_->SetMachineStatistic("keyboard_layout", "");
    statistics_provider_->SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kValid);
  }

 private:
  std::unique_ptr<system::FakeStatisticsProvider> statistics_provider_;
};

IN_PROC_BROWSER_TEST_P(CustomizationVPDTest, GetUILanguageList) {
  std::vector<std::string> locales = base::SplitString(
      GetParam(), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (std::string& l : locales) {
    base::TrimString(l, " ", &l);
  }
  EXPECT_EQ(locales,
            StartupCustomizationDocument::GetInstance()->configured_locales())
      << "Test failed for initial_locale='" << GetParam()
      << "', locales=" << Print(locales);

  auto ui_language_list =
      GetUILanguageList(nullptr, "", input_method::InputMethodManager::Get());
  EXPECT_GE(ui_language_list.size(), locales.size())
      << "Test failed for initial_locale='" << GetParam() << "'";

  for (size_t i = 0; i < ui_language_list.size(); ++i) {
    base::Value::Dict* language_info = ui_language_list[i].GetIfDict();

    ASSERT_TRUE(language_info)
        << "Test failed for initial_locale='" << GetParam() << "', i=" << i;

    std::string* value = language_info->FindString("value");
    ASSERT_TRUE(value) << "Test failed for initial_locale='" << GetParam()
                       << "', i=" << i;

    if (i < locales.size()) {
      EXPECT_EQ(locales[i], *value)
          << "Test failed for initial_locale='" << GetParam() << "', i=" << i;
    } else {
      EXPECT_EQ(kMostRelevantLanguagesDivider, *value)
          << "Test failed for initial_locale='" << GetParam() << "', i=" << i;
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(StringSequence,
                         CustomizationVPDTest,
                         testing::ValuesIn(kVPDInitialLocales));

}  // namespace ash
