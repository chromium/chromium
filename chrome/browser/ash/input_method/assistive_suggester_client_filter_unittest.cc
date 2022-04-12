// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"

#include "base/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/get_browser_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

using EnabledSuggestions = AssistiveSuggesterSwitch::EnabledSuggestions;

base::RepeatingCallback<void(GetFocusedTabUrlCallback)> ReturnUrl(
    const std::string& url) {
  return base::BindLambdaForTesting([url](GetFocusedTabUrlCallback callback) {
    std::move(callback).Run(GURL(url));
  });
}

struct VerifySuggesterTestCase {
  std::string test_name;
  std::string url;
  EnabledSuggestions enabled_suggestions;
};

using SuggesterAllowlist = testing::TestWithParam<VerifySuggesterTestCase>;

TEST_P(SuggesterAllowlist, VerifySuggesterAllowedState) {
  const VerifySuggesterTestCase& test_case = GetParam();
  AssistiveSuggesterClientFilter filter(ReturnUrl(test_case.url));
  EnabledSuggestions enabled_suggestions;

  filter.FetchEnabledSuggestionsThen(
      base::BindLambdaForTesting([&](const EnabledSuggestions& enabled) {
        enabled_suggestions = enabled;
      }));

  EXPECT_EQ(enabled_suggestions, test_case.enabled_suggestions);
}

INSTANTIATE_TEST_SUITE_P(
    AssistiveSuggesterClientFilterTest,
    SuggesterAllowlist,
    testing::ValuesIn<VerifySuggesterTestCase>({
        // Tests with https.
        {"DiscordHttps", "https://www.discord.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"MessengerHttps", "https://www.messenger.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"WhatsappHttps", "https://web.whatsapp.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"SkypeHttps", "https://web.skype.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = true}},
        {"DuoHttps", "https://duo.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"HangoutsHttps", "https://hangouts.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"MessagesHttps", "https://messages.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"TelegramHttps", "https://web.telegram.org",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"VoiceHttps", "https://voice.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"GmailHttps", "https://mail.google.com",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false}},
        {"DocsHttps", "https://docs.google.com",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false}},
        {"RandomHttps", "https://www.abc.com",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false}},

        // Tests with http.
        {"DiscordHttp", "http://www.discord.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"MessengerHttp", "http://www.messenger.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"WhatsappHttp", "http://web.whatsapp.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"SkypeHttp", "http://web.skype.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = true}},
        {"DuoHttp", "http://duo.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"HangoutsHttp", "http://hangouts.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"MessagesHttp", "http://messages.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"TelegramHttp", "http://web.telegram.org",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"VoiceHttp", "http://voice.google.com",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true}},
        {"GmailHttp", "http://mail.google.com",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false}},
        {"DocsHttp", "http://docs.google.com",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false}},
        {"RandomHttp", "http://www.abc.com",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false}},
    }),
    [](const testing::TestParamInfo<SuggesterAllowlist::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace input_method
}  // namespace ash
