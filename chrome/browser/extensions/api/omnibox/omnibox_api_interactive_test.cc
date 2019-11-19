// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/window_open_disposition.h"

namespace {

using base::ASCIIToUTF16;
using extensions::ResultCatcher;
using metrics::OmniboxEventProto;
using ui_test_utils::WaitForAutocompleteDone;

void InputKeys(Browser* browser, const std::vector<ui::KeyboardCode>& keys) {
  for (auto key : keys) {
    // Note that sending key presses can be flaky at times.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser, key, false, false,
                                                false, false));
  }
}

LocationBar* GetLocationBar(Browser* browser) {
  return browser->window()->GetLocationBar();
}

AutocompleteController* GetAutocompleteController(Browser* browser) {
  return GetLocationBar(browser)
      ->GetOmniboxView()
      ->model()
      ->autocomplete_controller();
}

base::string16 AutocompleteResultAsString(const AutocompleteResult& result) {
  std::string output(base::StringPrintf("{%" PRIuS "} ", result.size()));
  for (size_t i = 0; i < result.size(); ++i) {
    AutocompleteMatch match = result.match_at(i);
    std::string provider_name = match.provider->GetName();
    output.append(base::StringPrintf("[\"%s\" by \"%s\"] ",
                                     base::UTF16ToUTF8(match.contents).c_str(),
                                     provider_name.c_str()));
  }
  return base::UTF8ToUTF16(output);
}

using OmniboxApiTest = extensions::ExtensionApiTest;

}  // namespace

// http://crbug.com/167158
IN_PROC_BROWSER_TEST_F(OmniboxApiTest, DISABLED_Basic) {
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;

  // The results depend on the TemplateURLService being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  Profile* profile = browser()->profile();
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile));

  AutocompleteController* autocomplete_controller =
      GetAutocompleteController(browser());

  // Test that our extension's keyword is suggested to us when we partially type
  // it.
  {
    AutocompleteInput input(ASCIIToUTF16("keywor"),
                            metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile));
    autocomplete_controller->Start(input);
    WaitForAutocompleteDone(browser());
    EXPECT_TRUE(autocomplete_controller->done());

    // Now, peek into the controller to see if it has the results we expect.
    // First result should be to search for what was typed, second should be to
    // enter "extension keyword" mode.
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(2U, result.size()) << AutocompleteResultAsString(result);
    AutocompleteMatch match = result.match_at(0);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(match.deletable);

    match = result.match_at(1);
    EXPECT_EQ(ASCIIToUTF16("kw"), match.keyword);
  }

  // Test that our extension can send suggestions back to us.
  {
    AutocompleteInput input(ASCIIToUTF16("kw suggestio"),
                            metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile));
    autocomplete_controller->Start(input);
    WaitForAutocompleteDone(browser());
    EXPECT_TRUE(autocomplete_controller->done());

    // Now, peek into the controller to see if it has the results we expect.
    // First result should be to invoke the keyword with what we typed, 2-4
    // should be to invoke with suggestions from the extension, and the last
    // should be to search for what we typed.
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(5U, result.size()) << AutocompleteResultAsString(result);

    EXPECT_EQ(ASCIIToUTF16("kw"), result.match_at(0).keyword);
    EXPECT_EQ(ASCIIToUTF16("kw suggestio"), result.match_at(0).fill_into_edit);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_OTHER_ENGINE,
              result.match_at(0).type);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(0).provider->type());
    EXPECT_EQ(ASCIIToUTF16("kw"), result.match_at(1).keyword);
    EXPECT_EQ(ASCIIToUTF16("kw suggestion1"),
              result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(1).provider->type());
    EXPECT_EQ(ASCIIToUTF16("kw"), result.match_at(2).keyword);
    EXPECT_EQ(ASCIIToUTF16("kw suggestion2"),
              result.match_at(2).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(2).provider->type());
    EXPECT_EQ(ASCIIToUTF16("kw"), result.match_at(3).keyword);
    EXPECT_EQ(ASCIIToUTF16("kw suggestion3"),
              result.match_at(3).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(3).provider->type());

    base::string16 description =
        ASCIIToUTF16("Description with style: <match>, [dim], (url till end)");
    EXPECT_EQ(description, result.match_at(1).contents);
    ASSERT_EQ(6u, result.match_at(1).contents_class.size());

    EXPECT_EQ(0u, result.match_at(1).contents_class[0].offset);
    EXPECT_EQ(ACMatchClassification::NONE,
              result.match_at(1).contents_class[0].style);

    EXPECT_EQ(description.find('<'),
              result.match_at(1).contents_class[1].offset);
    EXPECT_EQ(ACMatchClassification::MATCH,
              result.match_at(1).contents_class[1].style);

    EXPECT_EQ(description.find('>') + 1u,
              result.match_at(1).contents_class[2].offset);
    EXPECT_EQ(ACMatchClassification::NONE,
              result.match_at(1).contents_class[2].style);

    EXPECT_EQ(description.find('['),
              result.match_at(1).contents_class[3].offset);
    EXPECT_EQ(ACMatchClassification::DIM,
              result.match_at(1).contents_class[3].style);

    EXPECT_EQ(description.find(']') + 1u,
              result.match_at(1).contents_class[4].offset);
    EXPECT_EQ(ACMatchClassification::NONE,
              result.match_at(1).contents_class[4].style);

    EXPECT_EQ(description.find('('),
              result.match_at(1).contents_class[5].offset);
    EXPECT_EQ(ACMatchClassification::URL,
              result.match_at(1).contents_class[5].style);

    AutocompleteMatch match = result.match_at(4);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_EQ(AutocompleteProvider::TYPE_SEARCH,
              result.match_at(4).provider->type());
    EXPECT_FALSE(match.deletable);
  }

  // Flaky, see http://crbug.com/167158
  /*
  {
    LocationBar* location_bar = GetLocationBar(browser());
    ResultCatcher catcher;
    OmniboxView* omnibox_view = location_bar->GetOmniboxView();
    omnibox_view->OnBeforePossibleChange();
    omnibox_view->SetUserText(ASCIIToUTF16("kw command"));
    omnibox_view->OnAfterPossibleChange(true);
    location_bar->AcceptInput();
    // This checks that the keyword provider (via javascript)
    // gets told to navigate to the string "command".
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
  */
}

IN_PROC_BROWSER_TEST_F(OmniboxApiTest, OnInputEntered) {
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;
  Profile* profile = browser()->profile();
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile));

  LocationBar* location_bar = GetLocationBar(browser());
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  ResultCatcher catcher;
  AutocompleteController* autocomplete_controller =
      GetAutocompleteController(browser());
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(ASCIIToUTF16("kw command"));
  omnibox_view->OnAfterPossibleChange(true);

  {
    AutocompleteInput input(ASCIIToUTF16("kw command"),
                            metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile));
    autocomplete_controller->Start(input);
  }
  omnibox_view->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(ASCIIToUTF16("kw newtab"));
  omnibox_view->OnAfterPossibleChange(true);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  {
    AutocompleteInput input(ASCIIToUTF16("kw newtab"),
                            metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile));
    autocomplete_controller->Start(input);
  }
  omnibox_view->model()->AcceptInput(WindowOpenDisposition::NEW_FOREGROUND_TAB);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that we get suggestions from and send input to the incognito context
// of an incognito split mode extension.
// http://crbug.com/100927
// Test is flaky: http://crbug.com/101219
IN_PROC_BROWSER_TEST_F(OmniboxApiTest, DISABLED_IncognitoSplitMode) {
  Profile* profile = browser()->profile();
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(profile->GetOffTheRecordProfile());

  ASSERT_TRUE(RunExtensionTestIncognito("omnibox")) << message_;

  // Open an incognito window and wait for the incognito extension process to
  // respond.
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(catcher_incognito.GetNextResult()) << catcher_incognito.message();

  // The results depend on the TemplateURLService being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  LocationBar* location_bar = GetLocationBar(incognito_browser);
  AutocompleteController* autocomplete_controller =
      GetAutocompleteController(incognito_browser);

  // Test that we get the incognito-specific suggestions.
  {
    AutocompleteInput input(ASCIIToUTF16("kw suggestio"),
                            metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile));
    autocomplete_controller->Start(input);
    WaitForAutocompleteDone(browser());
    EXPECT_TRUE(autocomplete_controller->done());

    // First result should be to invoke the keyword with what we typed, 2-4
    // should be to invoke with suggestions from the extension, and the last
    // should be to search for what we typed.
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(5U, result.size()) << AutocompleteResultAsString(result);
    ASSERT_FALSE(result.match_at(0).keyword.empty());
    EXPECT_EQ(ASCIIToUTF16("kw suggestion3 incognito"),
              result.match_at(3).fill_into_edit);
  }

  // Test that our input is sent to the incognito context. The test will do a
  // text comparison and succeed only if "command incognito" is sent to the
  // incognito context.
  {
    ResultCatcher catcher;
    AutocompleteInput input(ASCIIToUTF16("kw command incognito"),
                            metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile));
    autocomplete_controller->Start(input);
    location_bar->AcceptInput();
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

// Tests that the autocomplete popup doesn't reopen after accepting input for
// a given query.
// http://crbug.com/88552
IN_PROC_BROWSER_TEST_F(OmniboxApiTest, PopupStaysClosed) {
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;

  // The results depend on the TemplateURLService being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  Profile* profile = browser()->profile();
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile));

  LocationBar* location_bar = GetLocationBar(browser());
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      GetAutocompleteController(browser());
  OmniboxPopupModel* popup_model = omnibox_view->model()->popup_model();

  // Input a keyword query and wait for suggestions from the extension.
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(base::ASCIIToUTF16("kw comman"));
  omnibox_view->OnAfterPossibleChange(true);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  EXPECT_TRUE(popup_model->IsOpen());

  // Quickly type another query and accept it before getting suggestions back
  // for the query. The popup will close after accepting input - ensure that it
  // does not reopen when the extension returns its suggestions.
  extensions::ResultCatcher catcher;

  // TODO: Rather than send this second request by talking to the controller
  // directly, figure out how to send it via the proper calls to
  // location_bar or location_bar->().
  AutocompleteInput input(base::ASCIIToUTF16("kw command"),
                          metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile));
  autocomplete_controller->Start(input);
  location_bar->AcceptInput();
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  // This checks that the keyword provider (via javascript)
  // gets told to navigate to the string "command".
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(popup_model->IsOpen());
}

// Tests deleting a deletable omnibox extension suggestion result.
// Flaky on Windows. https://crbug.com/801316
#if defined(OS_WIN)
#define MAYBE_DeleteOmniboxSuggestionResult \
  DISABLED_DeleteOmniboxSuggestionResult
#else
#define MAYBE_DeleteOmniboxSuggestionResult DeleteOmniboxSuggestionResult
#endif
IN_PROC_BROWSER_TEST_F(OmniboxApiTest, MAYBE_DeleteOmniboxSuggestionResult) {
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;

  // The results depend on the TemplateURLService being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  Profile* profile = browser()->profile();
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile));

  AutocompleteController* autocomplete_controller =
      GetAutocompleteController(browser());

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Input a keyword query and wait for suggestions from the extension.
  InputKeys(browser(), {ui::VKEY_K, ui::VKEY_W, ui::VKEY_SPACE, ui::VKEY_D});

  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  // Peek into the controller to see if it has the results we expect.
  const AutocompleteResult& result = autocomplete_controller->result();
  ASSERT_EQ(4U, result.size()) << AutocompleteResultAsString(result);

  EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(0).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(0).provider->type());
  EXPECT_FALSE(result.match_at(0).deletable);

  EXPECT_EQ(base::ASCIIToUTF16("kw n1"), result.match_at(1).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(1).provider->type());
  // Verify that the first omnibox extension suggestion is deletable.
  EXPECT_TRUE(result.match_at(1).deletable);

  EXPECT_EQ(base::ASCIIToUTF16("kw n2"), result.match_at(2).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(2).provider->type());
  // Verify that the second omnibox extension suggestion is not deletable.
  EXPECT_FALSE(result.match_at(2).deletable);

  EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(3).fill_into_edit);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            result.match_at(3).type);
  EXPECT_FALSE(result.match_at(3).deletable);

// This test portion is excluded from Mac because the Mac key combination
// FN+SHIFT+DEL used to delete an omnibox suggestion cannot be reproduced.
// This is because the FN key is not supported in interactive_test_util.h.
#if !defined(OS_MACOSX)
  ExtensionTestMessageListener delete_suggestion_listener(
      "onDeleteSuggestion: des1", false);

  // Skip the first suggestion result.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  // Delete the second suggestion result. On Linux, this is done via SHIFT+DEL.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DELETE, false,
                                              true, false, false));
  // Verify that the onDeleteSuggestion event was fired.
  ASSERT_TRUE(delete_suggestion_listener.WaitUntilSatisfied());

  // Verify that the first suggestion result was deleted. There should be one
  // less suggestion result, 3 now instead of 4.
  ASSERT_EQ(3U, result.size());
  EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(0).fill_into_edit);
  EXPECT_EQ(base::ASCIIToUTF16("kw n2"), result.match_at(1).fill_into_edit);
  EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(2).fill_into_edit);
#endif
}

// Tests typing something but not staying in keyword mode.
IN_PROC_BROWSER_TEST_F(OmniboxApiTest, ExtensionSuggestionsOnlyInKeywordMode) {
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;

  // The results depend on the TemplateURLService being loaded. Make sure it is
  // loaded so that the autocomplete results are consistent.
  Profile* profile = browser()->profile();
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile));

  AutocompleteController* autocomplete_controller =
      GetAutocompleteController(browser());

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Input a keyword query and wait for suggestions from the extension.
  InputKeys(browser(), {ui::VKEY_K, ui::VKEY_W, ui::VKEY_SPACE, ui::VKEY_D});
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  // Peek into the controller to see if it has the results we expect.
  {
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(4U, result.size()) << AutocompleteResultAsString(result);

    EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(0).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(0).provider->type());

    EXPECT_EQ(base::ASCIIToUTF16("kw n1"), result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(1).provider->type());

    EXPECT_EQ(base::ASCIIToUTF16("kw n2"), result.match_at(2).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(2).provider->type());

    EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(3).fill_into_edit);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
              result.match_at(3).type);
  }

  // Now clear the omnibox by pressing escape multiple times, focus the
  // omnibox, then type the same text again except with a backspace after
  // the "kw ".  This will cause leaving keyword mode so the full text will be
  // "kw d" without a keyword chip displayed.  This middle step of focussing
  // the omnibox is necessary because on Mac pressing escape can make the
  // omnibox lose focus.
  InputKeys(browser(), {ui::VKEY_ESCAPE, ui::VKEY_ESCAPE});
  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  InputKeys(browser(), {ui::VKEY_K, ui::VKEY_W, ui::VKEY_SPACE, ui::VKEY_BACK,
                        ui::VKEY_D});

  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  // Peek into the controller to see if it has the results we expect.  Since
  // the user left keyword mode, the extension should not be the top-ranked
  // match nor should it have provided suggestions.  (It can and should provide
  // a match to query the extension for exactly what the user typed however.)
  {
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(2U, result.size()) << AutocompleteResultAsString(result);

    EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(0).fill_into_edit);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
              result.match_at(0).type);

    EXPECT_EQ(base::ASCIIToUTF16("kw d"), result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(1).provider->type());
  }
}
