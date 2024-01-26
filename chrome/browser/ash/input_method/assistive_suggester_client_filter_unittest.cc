// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/get_current_window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

using EnabledSuggestions = AssistiveSuggesterSwitch::EnabledSuggestions;

base::RepeatingCallback<void(GetFocusedTabUrlCallback)> ReturnUrl(
    const std::string& url) {
  return base::BindLambdaForTesting([url](GetFocusedTabUrlCallback callback) {
    std::optional<GURL> gurl =
        url.empty() ? std::nullopt : std::optional<GURL>(GURL(url));
    std::move(callback).Run(gurl);
  });
}

base::RepeatingCallback<WindowProperties(void)> ReturnWindowProperty(
    const WindowProperties& window_properties) {
  return base::BindLambdaForTesting(
      [window_properties]() { return window_properties; });
}

struct VerifySuggesterTestCase {
  std::string test_name;
  std::string url;
  std::string app_id;
  std::string arc_package_name;
  EnabledSuggestions enabled_suggestions;
};

class SuggesterContextBasedTest : public testing::Test {
 protected:
  SuggesterContextBasedTest() {}
};

TEST_F(SuggesterContextBasedTest, NoDiacriticsInPassword) {
  AssistiveSuggesterClientFilter filter(ReturnUrl("https://www.discord.com"),
                                        ReturnWindowProperty({}));
  EnabledSuggestions enabled_suggestions;

  filter.FetchEnabledSuggestionsThen(
      base::BindLambdaForTesting([&](const EnabledSuggestions& enabled) {
        enabled_suggestions = enabled;
      }),
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_PASSWORD));

  EnabledSuggestions expected = {.emoji_suggestions = true,
                                 .multi_word_suggestions = true,
                                 .personal_info_suggestions = true,
                                 .diacritic_suggestions = false};
  EXPECT_EQ(enabled_suggestions, expected);
}

TEST_F(SuggesterContextBasedTest, YesDiacriticsNormally) {
  AssistiveSuggesterClientFilter filter(ReturnUrl("https://www.discord.com"),
                                        ReturnWindowProperty({}));
  EnabledSuggestions enabled_suggestions;

  filter.FetchEnabledSuggestionsThen(
      base::BindLambdaForTesting([&](const EnabledSuggestions& enabled) {
        enabled_suggestions = enabled;
      }),
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));

  EnabledSuggestions expected = {.emoji_suggestions = true,
                                 .multi_word_suggestions = true,
                                 .personal_info_suggestions = true,
                                 .diacritic_suggestions = true};
  EXPECT_EQ(enabled_suggestions, expected);
}

using SuggesterAllowlist = testing::TestWithParam<VerifySuggesterTestCase>;

TEST_P(SuggesterAllowlist, VerifySuggesterAllowedState) {
  const VerifySuggesterTestCase& test_case = GetParam();
  AssistiveSuggesterClientFilter filter(
      ReturnUrl(test_case.url),
      ReturnWindowProperty({
          .app_id = test_case.app_id,
          .arc_package_name = test_case.arc_package_name,
      }));
  EnabledSuggestions enabled_suggestions;

  filter.FetchEnabledSuggestionsThen(
      base::BindLambdaForTesting([&](const EnabledSuggestions& enabled) {
        enabled_suggestions = enabled;
      }),
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_NONE));

  EXPECT_EQ(enabled_suggestions, test_case.enabled_suggestions);
}

INSTANTIATE_TEST_SUITE_P(
    AssistiveSuggesterClientFilterTest,
    SuggesterAllowlist,
    testing::ValuesIn<VerifySuggesterTestCase>({
        // Tests with https.
        {"DiscordHttps", /* url=*/"https://www.discord.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"MessengerHttps", /* url=*/"https://www.messenger.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"WhatsappHttps", /* url=*/"https://web.whatsapp.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"SkypeHttps", /* url=*/"https://web.skype.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"DuoHttps", /* url=*/"https://duo.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"HangoutsHttps", /* url=*/"https://hangouts.google.com",
         /* app_id=*/"", /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"MessagesHttps", /* url=*/"https://messages.google.com",
         /* app_id=*/"", /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"TelegramHttps", /* url=*/"https://web.telegram.org", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"VoiceHttps", /* url=*/"https://voice.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"DocsHttps", /* url=*/"https://docs.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"RandomHttps", /* url=*/"https://www.abc.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailHttps", /* url=*/"https://mail.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailWithPartialMailPathHttps",
         /* url=*/"https://mail.google.com/mail", "", "",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailWithFullMailPathHttps",
         /* url=*/"https://mail.google.com/mail/u/0", "", "",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailWithPartialChatPathHttps",
         /* url=*/"https://mail.google.com/chat", "", "",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"GmailWithFullChatPathHttps",
         /* url=*/"https://mail.google.com/chat/u/0/#chat/space/ABC123",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},

        // Tests with http.
        {"DiscordHttp", /* url=*/"http://www.discord.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"MessengerHttp", /* url=*/"http://www.messenger.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"WhatsappHttp", /* url=*/"http://web.whatsapp.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"SkypeHttp", /* url=*/"http://web.skype.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"DuoHttp", /* url=*/"http://duo.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"HangoutsHttp", /* url=*/"http://hangouts.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"MessagesHttp", /* url=*/"http://messages.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"TelegramHttp", /* url=*/"http://web.telegram.org", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"VoiceHttp", /* url=*/"http://voice.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"DocsHttp", /* url=*/"http://docs.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"RandomHttp", /* url=*/"http://www.abc.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailHttp", /* url=*/"http://mail.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailWithPartialMailPathHttp", /* url=*/"http://mail.google.com/mail",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailWithFullMailPathHttp",
         /* url=*/"http://mail.google.com/mail/u/0",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GmailWithPartialChatPathHttp", /* url=*/"http://mail.google.com/chat",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"GmailWithFullChatPathHttp",
         /* url=*/"http://mail.google.com/chat/u/0/#chat/space/ABC123",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = true}},
        {"DocsWithFullPath",
         /* url=*/
         "https://docs.google.com/document/d/"
         "1lDx4FoCA30OOXc0O93ax52u-12k9xp4a08pk1V3msNc/edit",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"SlidesWithFullPath",
         /* url=*/
         "https://docs.google.com/presentation/d/"
         "1sIuJA8CW9PthDMlJ9YLo7J2txlfQsauWVtZP44IDWn4/edit?parameters=1312312",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"SystemTextApp",
         /* url=*/"chrome-extension://mmfbcljfglbokpmkimbfghdkjmjhdgbg",
         /* app_id=*/"mmfbcljfglbokpmkimbfghdkjmjhdgbg",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = true,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = true,
                            .diacritic_suggestions = false}},
        {"ChromeTerminalViaUrl",
         /* url=*/"chrome-untrusted://terminal/",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        {"ChromeCroshViaUrl",
         /* url=*/"chrome-untrusted://crosh/",
         /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        {"ChromeSSHApp",
         /* url=*/"chrome-extension://iodihamcpbpeioajjeobimgagajmlibd",
         /* app_id=*/"iodihamcpbpeioajjeobimgagajmlibd",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        {"ChromeSSHDevApp",
         /* url=*/"chrome-extension://algkcnfjnajfhgimadimbjhmpaeohhln",
         /* app_id=*/"algkcnfjnajfhgimadimbjhmpaeohhln",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        {"CroshApp",
         /* url=*/"chrome-extension://cgfnfgkafmcdkdgilmojlnaadileaach",
         /* app_id=*/"cgfnfgkafmcdkdgilmojlnaadileaach",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        {"ChromeTerminalApp",
         /* url=*/"chrome-extension://fhicihalidkgcimdmhpohldehjmcabcf",
         /* app_id=*/"fhicihalidkgcimdmhpohldehjmcabcf",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        // TODO(b/245469813): Investigate if denied is intentional:
        // These tests currently exist because of the allowlists for arc++ apps
        // that imply these should be run. However, our current logic disables
        // all suggesters if there is no url.
        {"DiscordApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.discord",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"OrcaFacebookApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.facebook.orca",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"WhatsappApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.whatsapp",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"RaiderSkypeApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.skype.raider",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"TachyonGoogleApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.google.android.apps.tachyon",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"TalkGoogleApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.google.android.talk",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"TelegramApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"org.telegram.messenger",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"TextNowEnflickApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.enflick.android.TextNow",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"MliteFacebookApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.facebook.mlite",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"VoipViberApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.viber.voip",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"M2SkypeApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.skype.m2",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"ImoimImoApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.imo.android.imoim",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"GooglevoiceGoogleApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.google.android.apps.googlevoice",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"MobilemessengerPlaystationApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.playstation.mobilemessenger",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"KikApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"kik.android",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"LinkApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.link.messages.sms",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"NaverApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"jp.naver.line.android",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"HappybitsApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"co.happybits.marcopolo",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"ImoApp", /* url=*/"", /* app_id=*/"",
         /* arc_package_name=*/"com.imo.android.imous",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = true}},
        {"Cider", /* url=*/"https://cider.corp.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = false,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        {"Cider_v", /* url=*/"https://cider-v.corp.google.com", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
        {"Localhost", /* url=*/"http://localhost/some_url", /* app_id=*/"",
         /* arc_package_name=*/"",
         EnabledSuggestions{.emoji_suggestions = false,
                            .multi_word_suggestions = true,
                            .personal_info_suggestions = false,
                            .diacritic_suggestions = false}},
    }),
    [](const testing::TestParamInfo<SuggesterAllowlist::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace input_method
}  // namespace ash
