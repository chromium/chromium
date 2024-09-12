// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
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
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/window_open_disposition.h"

namespace extensions {

namespace {

using base::ASCIIToUTF16;
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

std::u16string AutocompleteResultAsString(const AutocompleteResult& result) {
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

struct ExpectedMatchComponent {
  std::u16string text;
  ACMatchClassification::Style style;
};
using ExpectedMatchComponents = std::vector<ExpectedMatchComponent>;

// A helper method to verify the expected styled components of an autocomplete
// match.
// TODO(devlin): Update other tests to use this handy check.
void VerifyMatchComponents(const ExpectedMatchComponents& expected,
                           const AutocompleteMatch& match) {
  std::u16string expected_string;
  for (const auto& component : expected)
    expected_string += component.text;

  EXPECT_EQ(expected_string, match.contents);

  // Check if we have the right number of components. If we don't, safely bail
  // so that we don't access out-of-bounds elements.
  if (expected.size() != match.contents_class.size()) {
    ADD_FAILURE() << "Improper number of components: " << expected.size()
                  << " vs " << match.contents_class.size();
    return;
  }

  // Iterate over the string and match each component.
  size_t curr_offset = 0;
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(expected[i].text);

    EXPECT_EQ(curr_offset, match.contents_class[i].offset);
    EXPECT_EQ(expected[i].style, match.contents_class[i].style);
    curr_offset += expected[i].text.size();
  }
}

using ContextType = ExtensionBrowserTest::ContextType;

class OmniboxApiTest : public ExtensionApiTest,
                       public testing::WithParamInterface<ContextType> {
 public:
  OmniboxApiTest() : ExtensionApiTest(GetParam()) {}
  ~OmniboxApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // The omnibox suggestion results depend on the TemplateURLService being
    // loaded. Make sure it is loaded so that the autocomplete results are
    // consistent.
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(profile()));
  }

  // Helper functions to retrieve the AutocompleteController for the Browser
  // created with the test (`browser()`) or a specific supplied `browser`.
  AutocompleteController* GetAutocompleteController() {
    return GetAutocompleteControllerForBrowser(browser());
  }
  AutocompleteController* GetAutocompleteControllerForBrowser(
      Browser* browser) {
    return GetLocationBar(browser)
        ->GetOmniboxView()
        ->controller()
        ->autocomplete_controller();
  }
};

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         OmniboxApiTest,
                         testing::Values(ContextType::kServiceWorker));
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         OmniboxApiTest,
                         testing::Values(ContextType::kPersistentBackground));

using OmniboxApiBackgroundPageTest = OmniboxApiTest;

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxApiBackgroundPageTest,
                         testing::Values(ContextType::kNone));

}  // namespace

// TODO(crbug.com/326903502): Flaky on TSan.
#if defined(THREAD_SANITIZER)
#define MAYBE_SendSuggestions DISABLED_SendSuggestions
#else
#define MAYBE_SendSuggestions SendSuggestions
#endif
IN_PROC_BROWSER_TEST_P(OmniboxApiTest, MAYBE_SendSuggestions) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  constexpr char kBackground[] =
      R"(chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           let richDescription =
               'Description with style: <match>&lt;match&gt;</match>, ' +
               '<dim>[dim]</dim>, <url>(url)</url>';
           let simpleDescription = 'simple description';
           suggest([
             {content: text + ' first', description: richDescription},
             {content: text + ' second', description: simpleDescription},
             {content: text + ' third', description: simpleDescription},
           ]);
         });)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  // Test that our extension's keyword is suggested to us when we partially type
  // it.
  {
    AutocompleteInput input(u"alph", metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile()));
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
    EXPECT_EQ(u"alpha", match.keyword);
  }

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"alpha input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  // Now, peek into the controller to see if it has the results we expect.
  // First result should be to invoke the keyword with what we typed, 2-4
  // should be to invoke with suggestions from the extension.
  const AutocompleteResult& result = autocomplete_controller->result();
  ASSERT_EQ(4U, result.size()) << AutocompleteResultAsString(result);

  // Invoke the keyword with what we typed.
  EXPECT_EQ(u"alpha", result.match_at(0).keyword);
  EXPECT_EQ(u"alpha input", result.match_at(0).fill_into_edit);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_OTHER_ENGINE,
            result.match_at(0).type);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(0).provider->type());

  // First suggestion, complete with rich description.
  {
    EXPECT_EQ(u"alpha", result.match_at(1).keyword);
    EXPECT_EQ(u"alpha input first", result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(1).provider->type());

    std::u16string rich_description =
        u"Description with style: <match>, [dim], (url)";
    EXPECT_EQ(rich_description, result.match_at(1).contents);
    const ExpectedMatchComponents expected_components = {
        {u"Description with style: ", ACMatchClassification::NONE},
        {u"<match>", ACMatchClassification::MATCH},
        {u", ", ACMatchClassification::NONE},
        {u"[dim]", ACMatchClassification::DIM},
        {u", ", ACMatchClassification::NONE},
        {u"(url)", ACMatchClassification::URL},
    };
    VerifyMatchComponents(expected_components, result.match_at(1));
  }

  // Second and third suggestions, with simple descriptions.
  {
    std::u16string simple_description = u"simple description";
    const ExpectedMatchComponents expected_components = {
        {simple_description, ACMatchClassification::NONE},
    };

    EXPECT_EQ(u"alpha", result.match_at(2).keyword);
    EXPECT_EQ(u"alpha input second", result.match_at(2).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(2).provider->type());
    EXPECT_EQ(simple_description, result.match_at(2).contents);
    VerifyMatchComponents(expected_components, result.match_at(2));

    EXPECT_EQ(u"alpha", result.match_at(3).keyword);
    EXPECT_EQ(u"alpha input third", result.match_at(3).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(3).provider->type());
    EXPECT_EQ(simple_description, result.match_at(3).contents);
    VerifyMatchComponents(expected_components, result.match_at(3));
  }
}

IN_PROC_BROWSER_TEST_P(OmniboxApiTest, OnInputEntered) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  // This extension will collect input entered into the omnibox and pass it
  // to the browser when instructed.
  constexpr char kBackground[] =
      R"(let results = [];
         chrome.omnibox.onInputEntered.addListener((text, disposition) => {
           if (text == 'send results') {
             chrome.test.sendMessage(JSON.stringify(results));
             return;
           }
           results.push({text, disposition});
         });)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  LocationBar* location_bar = GetLocationBar(browser());
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  ResultCatcher catcher;
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  auto send_input = [this, autocomplete_controller, omnibox_view](
                        std::u16string input_string,
                        WindowOpenDisposition disposition) {
    AutocompleteInput input(input_string, metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile()));
    autocomplete_controller->Start(input);
    omnibox_view->model()->OpenSelection(base::TimeTicks(), disposition);
    WaitForAutocompleteDone(browser());
  };

  send_input(u"alpha current tab", WindowOpenDisposition::CURRENT_TAB);
  send_input(u"alpha new tab", WindowOpenDisposition::NEW_FOREGROUND_TAB);

  ExtensionTestMessageListener listener;
  send_input(u"alpha send results", WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  static constexpr char kExpectedResult[] =
      R"([{"text":"current tab","disposition":"currentTab"},)"
      R"({"text":"new tab","disposition":"newForegroundTab"}])";
  EXPECT_EQ(kExpectedResult, listener.message());
  EXPECT_TRUE(listener.had_user_gesture());
}

// Tests receiving suggestions from and sending input to the incognito context
// of an incognito split mode extension.
// Regression test for https://crbug.com/100927.
IN_PROC_BROWSER_TEST_P(OmniboxApiTest, IncognitoSplitMode) {
  static constexpr char kManifest[] =
      R"({
           "name": "SetDefaultSuggestion",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "incognito": "split",
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  static constexpr char kBackground[] =
      R"(let suggestionSuffix =
             chrome.extension.inIncognitoContext ?
                 ' incognito' :
                 ' onTheRecord';
         chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {content: text + suggestionSuffix, description: 'description'}
           ]);
         });

         chrome.omnibox.onInputEntered.addListener((text, disposition) => {
           chrome.test.sendMessage(text);
         });

         chrome.test.notifyPass();)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  // Create an incognito browser, and wait for the extension to load. Our
  // LoadExtension() method ensures the on-the-record background page has spun
  // up, but we need to explicitly wait for the incognito version.
  Browser* incognito_browser = CreateIncognitoBrowser();
  Profile* incognito_profile = incognito_browser->profile();
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(incognito_profile);
  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher_incognito.GetNextResult()) << catcher_incognito.message();

  AutocompleteController* incognito_controller =
      GetAutocompleteControllerForBrowser(incognito_browser);

  // Test that we get the incognito-specific suggestions.
  {
    AutocompleteInput input(
        u"alpha input", metrics::OmniboxEventProto::NTP,
        ChromeAutocompleteSchemeClassifier(incognito_profile));
    incognito_controller->Start(input);
    WaitForAutocompleteDone(incognito_browser);
    EXPECT_TRUE(incognito_controller->done());
  }

  // First result should be to invoke the keyword with what we typed, and the
  // second should be the provided suggestion from the extension.
  const AutocompleteResult& result = incognito_controller->result();
  ASSERT_EQ(2u, result.size());

  // First result.
  EXPECT_EQ(u"alpha", result.match_at(0).keyword);
  EXPECT_EQ(u"alpha input", result.match_at(0).fill_into_edit);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_OTHER_ENGINE,
            result.match_at(0).type);

  // Second result: incognito-specific.
  EXPECT_EQ(u"alpha", result.match_at(1).keyword);
  EXPECT_EQ(u"alpha input incognito", result.match_at(1).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(1).provider->type());

  // Split-mode test: Send different input to the on-the-record and off-the-
  // record profiles, and wait for a message from each. Verify that the
  // extension received the proper input in each context.
  ExtensionTestMessageListener on_the_record_listener;
  on_the_record_listener.set_browser_context(profile());

  ExtensionTestMessageListener incognito_listener;
  incognito_listener.set_browser_context(incognito_profile);

  {
    AutocompleteInput input(u"alpha word on the record",
                            metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile()));
    GetAutocompleteController()->Start(input);
    GetLocationBar(browser())->GetOmniboxView()->model()->OpenSelection();
  }
  {
    AutocompleteInput input(
        u"alpha word incognito", metrics::OmniboxEventProto::NTP,
        ChromeAutocompleteSchemeClassifier(incognito_profile));
    incognito_controller->Start(input);
    GetLocationBar(incognito_browser)
        ->GetOmniboxView()
        ->model()
        ->OpenSelection();
  }

  EXPECT_TRUE(on_the_record_listener.WaitUntilSatisfied());
  EXPECT_TRUE(incognito_listener.WaitUntilSatisfied());

  EXPECT_EQ("word on the record", on_the_record_listener.message());
  EXPECT_EQ("word incognito", incognito_listener.message());
}

// The test is flaky on Win10. crbug.com/1045731.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PopupStaysClosed DISABLED_PopupStaysClosed
#else
#define MAYBE_PopupStaysClosed PopupStaysClosed
#endif
// Tests that the autocomplete popup doesn't reopen after accepting input for
// a given query.
// http://crbug.com/88552
IN_PROC_BROWSER_TEST_P(OmniboxApiBackgroundPageTest, MAYBE_PopupStaysClosed) {
  ASSERT_TRUE(RunExtensionTest("omnibox")) << message_;

  LocationBar* location_bar = GetLocationBar(browser());
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  // Input a keyword query and wait for suggestions from the extension.
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"kw comman");
  omnibox_view->OnAfterPossibleChange(true);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Quickly type another query and accept it before getting suggestions back
  // for the query. The popup will close after accepting input - ensure that it
  // does not reopen when the extension returns its suggestions.
  ResultCatcher catcher;

  // TODO: Rather than send this second request by talking to the controller
  // directly, figure out how to send it via the proper calls to
  // location_bar or location_bar->().
  AutocompleteInput input(u"kw command", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  location_bar->GetOmniboxView()->model()->OpenSelection();
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  // This checks that the keyword provider (via javascript)
  // gets told to navigate to the string "command".
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());
}

// Tests deleting a deletable omnibox extension suggestion result.
// Flaky on Windows and Linux TSan. https://crbug.com/1287949
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER))
#define MAYBE_DeleteOmniboxSuggestionResult \
  DISABLED_DeleteOmniboxSuggestionResult
#else
#define MAYBE_DeleteOmniboxSuggestionResult DeleteOmniboxSuggestionResult
#endif
IN_PROC_BROWSER_TEST_P(OmniboxApiTest, MAYBE_DeleteOmniboxSuggestionResult) {
  static constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  static constexpr char kBackground[] =
      R"(chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {content: text + ' first', description: 'first description'},
             {
               content: text + ' second',
               description: 'second description',
               deletable: true,
             },
             {content: text + ' third', description: 'third description'},
           ]);
         });
         chrome.omnibox.onDeleteSuggestion.addListener((text) => {
           chrome.test.sendMessage('onDeleteSuggestion: ' + text);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"alpha input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  // Peek into the controller to see if it has the results we expect.
  const AutocompleteResult& result = autocomplete_controller->result();
  ASSERT_EQ(4u, result.size()) << AutocompleteResultAsString(result);

  EXPECT_EQ(u"alpha input", result.match_at(0).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(0).provider->type());
  EXPECT_FALSE(result.match_at(0).deletable);

  EXPECT_EQ(u"alpha input first", result.match_at(1).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(1).provider->type());
  EXPECT_FALSE(result.match_at(1).deletable);

  EXPECT_EQ(u"alpha input second", result.match_at(2).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(2).provider->type());
  EXPECT_TRUE(result.match_at(2).deletable);

  EXPECT_EQ(u"alpha input third", result.match_at(3).fill_into_edit);
  EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
            result.match_at(3).provider->type());
  EXPECT_FALSE(result.match_at(3).deletable);

  // This test portion is excluded from Mac because the Mac key combination
  // FN+SHIFT+DEL used to delete an omnibox suggestion cannot be reproduced.
  // This is because the FN key is not supported in interactive_test_util.h.
  // On (some?) platforms, there is also a navigable "x" in the suggestion that
  // we could use instead. However, this is more prone to UI churn, and mostly
  // tests functionality that should instead be tested as part of the omnibox
  // view. We should have sufficient Mac coverage here by ensuring the result
  // matches are marked as deletable (verified above).
#if !BUILDFLAG(IS_MAC)
  ExtensionTestMessageListener delete_suggestion_listener;

  // Skip the first (accept current input) and second (first extension-provided
  // suggestion) omnibox results.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));

  // Delete the second suggestion result. On non-Mac, this is done via
  // SHIFT+DEL.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DELETE, false,
                                              true, false, false));

  // Verify that the onDeleteSuggestion event was fired. When this happens, the
  // extension sends us a message.
  ASSERT_TRUE(delete_suggestion_listener.WaitUntilSatisfied());
  EXPECT_EQ("onDeleteSuggestion: second description",
            delete_suggestion_listener.message());

  // Verify that the second suggestion result was deleted. There should be one
  // less suggestion result, 3 now instead of 4 (accept current input and two
  // extension-provided suggestions).
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ(u"alpha input", result.match_at(0).fill_into_edit);
  EXPECT_EQ(u"alpha input first", result.match_at(1).fill_into_edit);
  EXPECT_EQ(u"alpha input third", result.match_at(2).fill_into_edit);
#endif
}

// Tests that if the user hits "backspace" (leaving the extension keyword mode),
// the extension suggestions are not sent.
// TODO(crbug.com/40839815): Flaky.
IN_PROC_BROWSER_TEST_P(OmniboxApiTest,
                       DISABLED_ExtensionSuggestionsOnlyInKeywordMode) {
  static constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "kw" },
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  static constexpr char kBackground[] =
      R"(chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           let simpleDescription = 'simple description';
           suggest([
             {content: text + ' first', description: simpleDescription},
             {content: text + ' second', description: simpleDescription},
           ]);
         });)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Input "kw d", triggering the extension, and then wait for suggestions.
  InputKeys(browser(), {ui::VKEY_K, ui::VKEY_W, ui::VKEY_SPACE, ui::VKEY_D});
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  // We expect two suggestions from the extension in addition to the regular
  // input and "search what you typed".
  {
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(4U, result.size()) << AutocompleteResultAsString(result);

    EXPECT_EQ(u"kw d", result.match_at(0).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(0).provider->type());

    EXPECT_EQ(u"kw d first", result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(1).provider->type());

    EXPECT_EQ(u"kw d second", result.match_at(2).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(2).provider->type());

    EXPECT_EQ(u"kw d", result.match_at(3).fill_into_edit);
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

    EXPECT_EQ(u"kw d", result.match_at(0).fill_into_edit);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
              result.match_at(0).type);

    EXPECT_EQ(u"kw d", result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(1).provider->type());
  }
}

IN_PROC_BROWSER_TEST_P(OmniboxApiTest, SetDefaultSuggestionFailures) {
  constexpr char kManifest[] =
      R"({
           "name": "SetDefaultSuggestion",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "word" },
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  constexpr char kBackground[] =
      R"(chrome.test.runTests([
           function testSetDefaultSuggestionThrowsWithContentField() {
             // Note: This test is mostly for historical benefit. Previously,
             // we had manual coverage to ensure developers did not pass an
             // object with `content`; now, this is handled by the bindings
             // system. There's no special handling in this API, but given the
             // historical behavior, we add extra coverage here.
             const invalidSuggestion = {
                 description: 'description',
                 content: 'content',
             };
             const expectedError = /Unexpected property: 'content'./;
             chrome.test.assertThrows(
                 chrome.omnibox.setDefaultSuggestion,
                 [invalidSuggestion],
                 expectedError);
             chrome.test.succeed();
           },
           async function invalidXml_NoClosingTag() {
             // Service worker-based and background page-based extensions behave
             // differently here. In the background page case, the description
             // is parsed synchronously in the renderer; this results in an
             // error being emitted that must be caught with a try/catch. In
             // service worker contexts, we parse the description
             // asynchronously from the browser, and the error is returned via
             // runtime.lastError.
             // Because of this difference, getting the emitted error is a bit
             // of a pain.
             let error = await new Promise((resolve) => {
               try {
                 chrome.omnibox.setDefaultSuggestion(
                     {description: '<tag> <match>match</match> world'},
                     () => {
                       resolve(chrome.runtime.lastError.message);
                     });
               } catch (e) {
                 resolve(e.message);
               }
             });
             let expectedError = /Opening and ending tag mismatch/;
             chrome.test.assertTrue(expectedError.test(error), error);
             chrome.test.succeed();
           }
         ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

// Flaky on Linux TSan. https://crbug.com/1304694
#if (BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER))
#define MAYBE_SetDefaultSuggestion DISABLED_SetDefaultSuggestion
#else
#define MAYBE_SetDefaultSuggestion SetDefaultSuggestion
#endif
IN_PROC_BROWSER_TEST_P(OmniboxApiTest, MAYBE_SetDefaultSuggestion) {
  constexpr char kManifest[] =
      R"({
           "name": "SetDefaultSuggestion",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "word" },
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  constexpr char kBackground[] =
      R"(chrome.test.runTests([
           function setDefaultSuggestion() {
             chrome.omnibox.setDefaultSuggestion(
                 {description: 'hello <match>match</match> world'},
                 () => {
                   chrome.test.succeed();
                 });
           }
         ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Input a keyword query and wait for suggestions from the extension.
  // Note that we need to add a character after the keyword for the service to
  // trigger the extension.
  InputKeys(browser(), {ui::VKEY_W, ui::VKEY_O, ui::VKEY_R, ui::VKEY_D,
                        ui::VKEY_SPACE, ui::VKEY_D});
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result = autocomplete_controller->result();
  ASSERT_EQ(1u, result.size()) << AutocompleteResultAsString(result);

  {
    const AutocompleteMatch& match = result.match_at(0);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_OTHER_ENGINE, match.type);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD, match.provider->type());

    // The "description" given by the extension is shown as the "contents" in
    // the AutocompleteMatch. The XML-marked string is
    // "hello <match>match</match> world", which is then shown as
    // "hello match world".
    // It should have 3 "components": an unstyled "hello ", a match-styled
    // "match", and an unstyled " world".
    const ExpectedMatchComponents expected_components = {
        {u"hello ", ACMatchClassification::NONE},
        {u"match", ACMatchClassification::MATCH},
        {u" world", ACMatchClassification::NONE},
    };
    VerifyMatchComponents(expected_components, match);
  }
}

// Tests an extension passing empty suggestions. Regression test for
// https://crbug.com/1330137.
// TODO(crbug.com/326903502): Flaky on TSan.
#if defined(THREAD_SANITIZER)
#define MAYBE_PassEmptySuggestions DISABLED_PassEmptySuggestions
#else
#define MAYBE_PassEmptySuggestions PassEmptySuggestions
#endif
IN_PROC_BROWSER_TEST_P(OmniboxApiTest, MAYBE_PassEmptySuggestions) {
  static constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 2,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "scripts": [ "background.js" ], "persistent": true }
         })";
  // Register a listener that passes back empty suggestions if there is no
  // text content.
  static constexpr char kBackground[] =
      R"(chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           let results = text.length > 0 ?
               [{content: "foo", description: "Foo"}] :
               [];
           suggest(results);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Enter "alpha d" into the omnibox to trigger the extension.
  InputKeys(browser(), {ui::VKEY_A, ui::VKEY_L, ui::VKEY_P, ui::VKEY_H,
                        ui::VKEY_A, ui::VKEY_SPACE, ui::VKEY_D});
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  {
    // We expect two results - sending the typed text to the extension,
    // and the single extension suggestion ("foo").
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(2u, result.size()) << AutocompleteResultAsString(result);

    EXPECT_EQ(u"alpha d", result.match_at(0).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(0).provider->type());

    EXPECT_EQ(u"alpha foo", result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(1).provider->type());
  }

  // Now, hit the backspace key, so that the only text is "alpha ". The
  // extension should still be receiving input.
  InputKeys(browser(), {ui::VKEY_BACK});

  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  {
    // The extension sent back an empty set of suggestions, so we expect
    // only two results - sending the typed text to the extension and searching
    // for what the user typed.
    const AutocompleteResult& result = autocomplete_controller->result();
    ASSERT_EQ(2u, result.size()) << AutocompleteResultAsString(result);

    EXPECT_EQ(u"alpha ", result.match_at(0).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(0).provider->type());

    AutocompleteMatch match = result.match_at(1);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_EQ(AutocompleteProvider::TYPE_SEARCH,
              result.match_at(1).provider->type());
  }
}

}  // namespace extensions
