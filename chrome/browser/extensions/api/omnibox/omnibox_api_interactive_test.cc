// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/test/base/autocomplete_change_observer.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/permissions/permissions_test_util.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace extensions {

namespace {

using base::ASCIIToUTF16;
using metrics::OmniboxEventProto;

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)

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

using ContextType = browser_test_util::ContextType;

class OmniboxApiTestBase : public ExtensionApiTest {
 public:
  explicit OmniboxApiTestBase(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~OmniboxApiTestBase() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // The omnibox suggestion results depend on the TemplateURLService being
    // loaded. Make sure it is loaded so that the autocomplete results are
    // consistent.
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(profile()));
  }

#if BUILDFLAG(IS_ANDROID)
  AutocompleteController* GetAutocompleteController() {
    return AutocompleteControllerAndroid::Factory::GetForProfile(profile())
        ->autocomplete_controller_for_test();
  }

  void WaitForAutocompleteDone() {
    AutocompleteController* controller = GetAutocompleteController();
    while (!controller->done()) {
      AutocompleteChangeObserver(profile()).Wait();
    }
  }
#else
  // Helper functions to retrieve the AutocompleteController for the Browser
  // created with the test (`browser()`) or a specific supplied `browser`.
  AutocompleteController* GetAutocompleteController() {
    return GetAutocompleteControllerForBrowser(browser());
  }

  AutocompleteController* GetAutocompleteControllerForBrowser(
      Browser* browser) {
    return GetLocationBar(browser)
        ->GetOmniboxController()
        ->autocomplete_controller();
  }

  void WaitForAutocompleteDone() {
    ui_test_utils::WaitForAutocompleteDone(browser());
  }
#endif  // BUILDFLAG(IS_ANDROID)
};

class OmniboxApiTest : public OmniboxApiTestBase,
                       public testing::WithParamInterface<ContextType> {
 public:
  OmniboxApiTest() : OmniboxApiTestBase(GetParam()) {}
  ~OmniboxApiTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         OmniboxApiTest,
                         testing::Values(ContextType::kServiceWorker));

// Desktop Android only supports service worker.
#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         OmniboxApiTest,
                         testing::Values(ContextType::kPersistentBackground));

using OmniboxApiBackgroundPageTest = OmniboxApiTest;

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxApiBackgroundPageTest,
                         testing::Values(ContextType::kNone));
#endif  // !BUILDFLAG(IS_ANDROID)

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

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  // Test that our extension's keyword is suggested to us when we partially type
  // it.
  {
    AutocompleteInput input(u"alph", metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile()));
    autocomplete_controller->Start(input);
    WaitForAutocompleteDone();
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
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  // Now, peek into the controller to see if it has the results we expect.
  // First result should be to invoke the keyword with what we typed, 2-4
  // should be to invoke with suggestions from the extension.
  const AutocompleteResult& result = autocomplete_controller->result();
  ASSERT_EQ(4U, result.size()) << AutocompleteResultAsString(result);
  int first_match_relevance = result.match_at(0).relevance;

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
    EXPECT_EQ(first_match_relevance - 1, result.match_at(1).relevance);

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
    EXPECT_EQ(first_match_relevance - 2, result.match_at(2).relevance);
    VerifyMatchComponents(expected_components, result.match_at(2));

    EXPECT_EQ(u"alpha", result.match_at(3).keyword);
    EXPECT_EQ(u"alpha input third", result.match_at(3).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_KEYWORD,
              result.match_at(3).provider->type());
    EXPECT_EQ(simple_description, result.match_at(3).contents);
    EXPECT_EQ(first_match_relevance - 3, result.match_at(3).relevance);
    VerifyMatchComponents(expected_components, result.match_at(3));
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/405219624): Port these tests to desktop Android. Most require
// access to the Views location bar, which is not available on Android.
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

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  LocationBar* location_bar = GetLocationBar(browser());
  ResultCatcher catcher;
  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  auto send_input = [this, autocomplete_controller, location_bar](
                        std::u16string input_string,
                        WindowOpenDisposition disposition) {
    AutocompleteInput input(input_string, metrics::OmniboxEventProto::NTP,
                            ChromeAutocompleteSchemeClassifier(profile()));
    autocomplete_controller->Start(input);
    location_bar->GetOmniboxController()->edit_model()->OpenSelectionForTesting(
        base::TimeTicks(), disposition);
    WaitForAutocompleteDone();
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
    ui_test_utils::WaitForAutocompleteDone(incognito_browser);
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
    GetLocationBar(browser())
        ->GetOmniboxController()
        ->edit_model()
        ->OpenSelectionForTesting();
  }
  {
    AutocompleteInput input(
        u"alpha word incognito", metrics::OmniboxEventProto::NTP,
        ChromeAutocompleteSchemeClassifier(incognito_profile));
    incognito_controller->Start(input);
    GetLocationBar(incognito_browser)
        ->GetOmniboxController()
        ->edit_model()
        ->OpenSelectionForTesting();
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
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());
  EXPECT_TRUE(location_bar->GetOmniboxController()->IsPopupOpen());

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

  location_bar->GetOmniboxController()->edit_model()->OpenSelectionForTesting();
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());
  // This checks that the keyword provider (via javascript)
  // gets told to navigate to the string "command".
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_FALSE(location_bar->GetOmniboxController()->IsPopupOpen());
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
  WaitForAutocompleteDone();
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

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Input "kw d", triggering the extension, and then wait for suggestions.
  InputKeys(browser(), {ui::VKEY_K, ui::VKEY_W, ui::VKEY_SPACE, ui::VKEY_D});
  WaitForAutocompleteDone();
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

  WaitForAutocompleteDone();
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
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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
             let expectedError = /(Opening and ending tag mismatch|Unexpected closing tag)/;
             chrome.test.assertTrue(expectedError.test(error), error);
             chrome.test.succeed();
           }
         ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/405219624): Port these tests to desktop Android. Most require
// access to the Views location bar, which is not available on Android.
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
  WaitForAutocompleteDone();
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
  WaitForAutocompleteDone();
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

  WaitForAutocompleteDone();
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
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class UnscopedOmniboxApiTest : public OmniboxApiTestBase {
 public:
  UnscopedOmniboxApiTest() {
    // TODO(crbug.com/441102004): Update UnscopedExtensionZeroSuggest to support
    //   kAiModeOmniboxEntryPoint.
    scoped_feature_list_.InitWithFeatures(
        {extensions_features::kExperimentalOmniboxLabs},
        {omnibox::kAiModeOmniboxEntryPoint});
  }

  // Helper function to set the stop timer duration for the autocomplete
  // controller.
  void SetStopTimerDuration(base::TimeDelta duration) {
    GetAutocompleteController()->config_.stop_timer_duration = duration;
  }

 private:
  void SetUpOnMainThread() override {
    OmniboxApiTestBase::SetUpOnMainThread();
    // Prevent the stop timer from killing the hints fetch early, which might
    // cause test flakiness due to timeout.
    SetStopTimerDuration(base::Seconds(30));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest,
                       UnscopedExtensionsUpdatedOnLoadAndUnload) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"()");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  TemplateURLService* turl_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  const ExtensionId& extension_id = extension->id();

  // The permission is grnated when loading an unpacked extension, so the
  // extension id should be added to the list of unscoped extensions.
  EXPECT_TRUE(
      turl_service->GetUnscopedModeExtensionIds().contains(extension_id));

  // The extension id should be removed from the list of extension ids when the
  // extension is unloaded.
  UnloadExtension(extension_id);
  EXPECT_FALSE(
      turl_service->GetUnscopedModeExtensionIds().contains(extension_id));
}

IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest,
                       RuntimePermissionChangesUpdateUnscopedExtensionsList) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "optional_permissions" : [ "omnibox.directInput" ]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"()");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  TemplateURLService* turl_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  const ExtensionId& extension_id = extension->id();

  // The permission is not granted, so the extension id should not be added to
  // the list of unscoped mode extensions (i.e. extensions that do not requuire
  // keyword mode).
  EXPECT_FALSE(
      turl_service->GetUnscopedModeExtensionIds().contains(extension_id));

  // When the permission is granted, the list should now contain the extension
  // id.
  APIPermissionSet api_permissions;
  api_permissions.insert(mojom::APIPermissionID::kOmniboxDirectInput);
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                    URLPatternSet(), URLPatternSet()));
  EXPECT_TRUE(
      turl_service->GetUnscopedModeExtensionIds().contains(extension_id));

  // If the permission is revoked again, the extension id should be removed from
  // the list.
  permissions_test_util::RevokeOptionalPermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(api_permissions.Clone(), ManifestPermissionSet(),
                    URLPatternSet(), URLPatternSet()),
      PermissionsUpdater::RemoveType::REMOVE_SOFT);
  EXPECT_FALSE(
      turl_service->GetUnscopedModeExtensionIds().contains(extension_id));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/405219624): Port these tests to desktop Android. Most require
// access to the Views location bar, which is not available on Android.
IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, UnscopedSendSuggestions) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";

  constexpr char kBackground[] =
      R"(chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           let richDescription =
               'Description with style: <match>&lt;match&gt;</match>, ' +
               '<dim>[dim]</dim>, <url>(url)</url>';
           let simpleDescription = 'simple description';
           suggest([
             {content: 'first', description: richDescription},
             {content: 'second', description: simpleDescription},
             {content: 'third', description: simpleDescription},
           ]);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result = autocomplete_controller->result();
  // Check if the 3 suggestions are received (+1 for the default search entry).
  ASSERT_EQ(4U, result.size()) << AutocompleteResultAsString(result);

  // First suggestion, complete with rich description.
  {
    EXPECT_EQ(u"first", result.match_at(1).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(1).provider->type());

    std::u16string rich_description =
        u"Description with style: <match>, [dim], (url)";
    EXPECT_EQ(rich_description, result.match_at(1).contents);
    EXPECT_EQ(result.match_at(1).provider->type(),
              AutocompleteProvider::TYPE_UNSCOPED_EXTENSION);
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

    EXPECT_EQ(u"second", result.match_at(2).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(2).provider->type());
    EXPECT_EQ(simple_description, result.match_at(2).contents);
    VerifyMatchComponents(expected_components, result.match_at(2));

    EXPECT_EQ(u"third", result.match_at(3).fill_into_edit);
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(3).provider->type());
    EXPECT_EQ(simple_description, result.match_at(3).contents);
    VerifyMatchComponents(expected_components, result.match_at(3));
  }
}

IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, UnscopedDeleteSuggestions) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";

  constexpr char kBackground[] =
      R"(chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {content: 'first', description: 'first description'},
             {
                content: 'second',
                description: 'second description',
                deletable: true,
              },
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

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result = autocomplete_controller->result();
  // Check if the 2 suggestions are received (+1 for the default search entry).
  ASSERT_EQ(3U, result.size()) << AutocompleteResultAsString(result);

  // First suggestion is not deletable.
  {
    EXPECT_EQ(u"first", result.match_at(1).fill_into_edit);
    EXPECT_FALSE(result.match_at(1).deletable);
  }

  // Second suggestion is deletable.
  {
    EXPECT_EQ(u"second", result.match_at(2).fill_into_edit);
    EXPECT_FALSE(result.match_at(1).deletable);
  }

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
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(u"input", result.match_at(0).fill_into_edit);
  EXPECT_EQ(u"first", result.match_at(1).fill_into_edit);
#endif
}

IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, OnInputEntered) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";
  // This extension will collect input entered into the omnibox and pass it
  // to the browser when instructed.
  constexpr char kBackground[] =
      R"(
         chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {content: text, description: 'description'}
           ]);
         });

         chrome.omnibox.onInputEntered.addListener((text, disposition) => {
           chrome.test.sendMessage(text);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener listener("sending input");
  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Send an input to the extension and wait for the sggestion to arrive before
  // we can select it.
  AutocompleteInput input(u"sending input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  ASSERT_TRUE(autocomplete_controller->done());

  LocationBar* location_bar = GetLocationBar(browser());

  // This is equivalent of the user arrowing down in the omnibox.
  // We need to select the second match because the first one is for the default
  // provider.
  location_bar->GetOmniboxController()->edit_model()->SetPopupSelection(
      OmniboxPopupSelection(1));

  // Select the suggestion created by the extension, which will trigger the
  // `onInputEntered` event.
  location_bar->GetOmniboxController()->edit_model()->OpenSelectionForTesting(
      base::TimeTicks(), WindowOpenDisposition::CURRENT_TAB);

  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("sending input", listener.message());
  EXPECT_TRUE(listener.had_user_gesture());
}

IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, UnscopedSuggestionGrouping) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";

  constexpr char kBackground[] =
      R"(
         chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {content: 'first', description: 'description'}
           ]);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result = autocomplete_controller->result();
  // Check if the suggestion is received (+1 for the default search entry).
  ASSERT_EQ(2U, result.size()) << AutocompleteResultAsString(result);

  // Second suggestion is given the first extension group and has a header of
  // "alpha".
  {
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(1).provider->type());
    EXPECT_EQ(omnibox::GROUP_UNSCOPED_EXTENSION_1,
              result.match_at(1).suggestion_group_id);
    EXPECT_EQ(u"alpha", result.GetHeaderForSuggestionGroup(
                            *result.match_at(1).suggestion_group_id));
  }
}

// Tests that unscoped extensions are limited to sending four suggestions.
IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, LimitSuggestions) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";

  constexpr char kBackground[] =
      R"(
         chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {content: 'first', description: 'description'},
             {content: 'second', description: 'description'},
             {content: 'third', description: 'description'},
             {content: 'fourth', description: 'description'},
             {content: 'fifth', description: 'description'},
             {content: 'sixth', description: 'description'},
           ]);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result = autocomplete_controller->result();
  // Check if 4 suggestions are received (+1 for the default search entry).
  ASSERT_EQ(5U, result.size()) << AutocompleteResultAsString(result);

  // Second suggestion is given the first extension group and has a header of
  // "alpha".
  {
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(1).provider->type());
    EXPECT_EQ(u"alpha", result.GetHeaderForSuggestionGroup(
                            *result.match_at(1).suggestion_group_id));
    EXPECT_EQ(u"first", result.match_at(1).fill_into_edit);
  }
  {
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(4).provider->type());
    EXPECT_EQ(omnibox::GROUP_UNSCOPED_EXTENSION_1,
              result.match_at(4).suggestion_group_id);
    EXPECT_EQ(u"fourth", result.match_at(4).fill_into_edit);
  }
}

// Tests that extensions can add actions to Omnibox suggestions and that the
// corresponding `OnActionExecuted` event is triggered when the user clicks on
// the action button.
IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, OnActionExecuted) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Action",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";
  // This extension will create a suggestion with an action and handle action
  // execution events.
  constexpr char kBackground[] =
      R"(
         chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {
               content: text,
               description: 'description',
               actions: [{
                 name: 'do_something',
                 label: 'Do something',
                 tooltipText: 'Do something the user wants'
               }]
             }
           ]);
         });

         chrome.omnibox.onActionExecuted.addListener((actionExecution) => {
           chrome.test.sendMessage(
               actionExecution.actionName + "-" + actionExecution.content);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener listener("do_something-sending input");
  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Send an input to the extension and wait for the sggestion to arrive before
  // we can select it.
  AutocompleteInput input(u"sending input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  ASSERT_TRUE(autocomplete_controller->done());

  LocationBar* location_bar = GetLocationBar(browser());

  // This is equivalent of the user clicking on the action added to the first
  // omnibox suggestion created by the extension.
  location_bar->GetOmniboxController()->edit_model()->OpenSelection(
      OmniboxPopupSelection(
          /*line=*/1, /*state=*/OmniboxPopupSelection::FOCUSED_BUTTON_ACTION,
          /*action_index=*/0),
      base::TimeTicks(), WindowOpenDisposition::CURRENT_TAB);

  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("do_something-sending input", listener.message());
  EXPECT_TRUE(listener.had_user_gesture());
}

// Tests that extensions can add actions with custom icons to Omnibox
// suggestions.
IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, ActionIconAppliedToMatch) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Action with icon",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";
  // This extension will create a suggestion with an action that has a green
  // icon.
  constexpr char kBackground[] =
      R"(
         chrome.omnibox.onInputChanged.addListener((text, suggest) => {
          const canvas = new OffscreenCanvas(16, 16);
          const context = canvas.getContext('2d');
          context.fillStyle = '#00FF00';
          context.fillRect(0, 0, 16, 16);
           suggest([
             {
               content: text,
               description: 'description',
               actions: [{
                 name: 'do_something',
                 label: 'Do something',
                 tooltipText: 'Do something the user wants',
                 icon: context.getImageData(0, 0, 16, 16)
               }]
             }
           ]);
         });

         chrome.omnibox.onActionExecuted.addListener((actionExecution) => {
           chrome.test.sendMessage(
               actionExecution.actionName + "-" + actionExecution.content);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener listener("do_something-sending input");
  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Send an input to the extension and wait for the sggestion to arrive before
  // we can select it.
  AutocompleteInput input(u"sending input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  ASSERT_TRUE(autocomplete_controller->done());

  {
    const AutocompleteResult& result = autocomplete_controller->result();
    // First match  is for the default search entry, so directly check the
    // second match.
    AutocompleteMatch match = result.match_at(1);
    // Manually construct an all-green icon and compare to the action icon.
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 16);
    bitmap.eraseColor(SK_ColorGREEN);
    gfx::test::AreImagesEqual(match.actions[0]->GetIconImage(),
                              gfx::Image::CreateFrom1xBitmap(bitmap));
  }
}

// Tests that multiple unscoped extensions work at the same time and are
// displayed with different headers.
IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest, MultipleUnscopedExtensions) {
  constexpr char kManifest[] =
      R"({
          "name": "Basic Send Suggestions",
          "manifest_version": 3,
          "version": "0.1",
          "omnibox": { "keyword": "alpha" },
          "background": { "service_worker": "background.js"},
          "permissions" : [ "omnibox.directInput" ]
        })";

  constexpr char kManifest2[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "dog" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";

  constexpr char kBackground[] =
      R"(
        chrome.omnibox.onInputChanged.addListener((text, suggest) => {
          suggest([
            {content: 'first', description: 'description'}
          ]);
        });)";

  constexpr char kBackground2[] =
      R"(
        chrome.omnibox.onInputChanged.addListener((text, suggest) => {
          suggest([
            {content: 'second', description: 'description'}
          ]);
        });)";

  TestExtensionDir test_dir;
  TestExtensionDir test_dir2;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  test_dir2.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground2);
  test_dir2.WriteManifest(kManifest2);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  const Extension* extension2 = LoadExtension(test_dir2.UnpackedPath());
  ASSERT_TRUE(extension2);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Prevent the stop timer from killing the hints fetch early, which might
  // cause test flakiness due to timeout.
  SetStopTimerDuration(base::Seconds(20));

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"input", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result = autocomplete_controller->result();
  // Check if the suggestion is received (+1 for the default search entry).
  ASSERT_EQ(3U, result.size()) << AutocompleteResultAsString(result);

  // Each extension suggestion header should match the extension name that
  // it came from.
  std::set<std::u16string> extension_names = {u"alpha", u"dog"};
  {
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(1).provider->type());
    EXPECT_EQ(omnibox::GROUP_UNSCOPED_EXTENSION_1,
              result.match_at(1).suggestion_group_id);
    EXPECT_TRUE(base::Contains(extension_names,
                               result.GetHeaderForSuggestionGroup(
                                   *result.match_at(1).suggestion_group_id)));
    extension_names.erase(result.GetHeaderForSuggestionGroup(
        *result.match_at(1).suggestion_group_id));
  }
  {
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result.match_at(2).provider->type());
    EXPECT_EQ(omnibox::GROUP_UNSCOPED_EXTENSION_2,
              result.match_at(2).suggestion_group_id);
    EXPECT_TRUE(base::Contains(extension_names,
                               result.GetHeaderForSuggestionGroup(
                                   *result.match_at(2).suggestion_group_id)));
  }
}

// Test if unscoped suggestions send in zero suggest.
// TODO(crbug.com/409601761): Test is flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_UnscopedExtensionZeroSuggest DISABLED_UnscopedExtensionZeroSuggest
#else
#define MAYBE_UnscopedExtensionZeroSuggest UnscopedExtensionZeroSuggest
#endif
IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest,
                       MAYBE_UnscopedExtensionZeroSuggest) {
  constexpr char kManifest[] =
      R"({
           "name": "Basic Send Suggestions",
           "manifest_version": 3,
           "version": "0.1",
           "omnibox": { "keyword": "alpha" },
           "background": { "service_worker": "background.js"},
           "permissions" : [ "omnibox.directInput" ]
         })";

  // Don't trim whitespace before "first" to ensure unscoped extension provider
  // doesn't hit DCHECK in `SplitKeywordFromInput`.
  constexpr char kBackground[] =
      R"(
         chrome.omnibox.onInputChanged.addListener((text, suggest) => {
           suggest([
             {content: ' first', description: 'description'}
           ]);
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Test that our extension can send suggestions back to us on NTP.
  AutocompleteInput input_ntp(u"", metrics::OmniboxEventProto::NTP,
                              ChromeAutocompleteSchemeClassifier(profile()));
  input_ntp.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  autocomplete_controller->Start(input_ntp);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result_ntp = autocomplete_controller->result();
  // Check if the suggestion is received (+1 for the IPH).
  ASSERT_EQ(2U, result_ntp.size()) << AutocompleteResultAsString(result_ntp);

  // Second suggestion is given the first extension group and has a header of
  // "alpha".
  {
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result_ntp.match_at(0).provider->type());
    EXPECT_EQ(omnibox::GROUP_UNSCOPED_EXTENSION_1,
              result_ntp.match_at(0).suggestion_group_id);
    EXPECT_EQ(u"alpha", result_ntp.GetHeaderForSuggestionGroup(
                            *result_ntp.match_at(0).suggestion_group_id));
    EXPECT_EQ(u"first", result_ntp.match_at(0).fill_into_edit);
  }

  // Test that our extension can send suggestions back to us on SRP.
  AutocompleteInput input_srp(
      u"",
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      ChromeAutocompleteSchemeClassifier(profile()));
  input_srp.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  autocomplete_controller->Start(input_srp);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result_srp = autocomplete_controller->result();
  // Check if the suggestion is received.
  ASSERT_EQ(1U, result_srp.size()) << AutocompleteResultAsString(result_srp);

  // Second suggestion is given the first extension group and has a header of
  // "alpha".
  {
    EXPECT_EQ(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
              result_srp.match_at(0).provider->type());
    EXPECT_EQ(omnibox::GROUP_UNSCOPED_EXTENSION_1,
              result_srp.match_at(0).suggestion_group_id);
    EXPECT_EQ(u"alpha", result_srp.GetHeaderForSuggestionGroup(
                            *result_srp.match_at(0).suggestion_group_id));
  }
}

// Test if unscoped extension are grouped together in zps.
// TODO(crbug.com/409601761): Test is flaky on Linux.
// TODO(crbug.com/425974968): Test is flaky on ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MultipleUnscopedExtensionsZeroSuggest \
  DISABLED_MultipleUnscopedExtensionsZeroSuggest
#else
#define MAYBE_MultipleUnscopedExtensionsZeroSuggest \
  MultipleUnscopedExtensionsZeroSuggest
#endif
IN_PROC_BROWSER_TEST_F(UnscopedOmniboxApiTest,
                       MAYBE_MultipleUnscopedExtensionsZeroSuggest) {
  constexpr char kManifest[] =
      R"({
      "name": "Basic Send Suggestions",
      "manifest_version": 3,
      "version": "0.1",
      "omnibox": { "keyword": "alpha" },
      "background": { "service_worker": "background.js"},
      "permissions" : [ "omnibox.directInput" ]
    })";

  constexpr char kManifest2[] =
      R"({
        "name": "Basic Send Suggestions",
        "manifest_version": 3,
        "version": "0.1",
        "omnibox": { "keyword": "dog" },
        "background": { "service_worker": "background.js"},
        "permissions" : [ "omnibox.directInput" ]
      })";

  constexpr char kBackground[] =
      R"(
    chrome.omnibox.onInputChanged.addListener((text, suggest) => {
      suggest([
        {content: 'first', description: 'extension1'},
        {content: 'second', description: 'extension1'}
      ]);
    });)";

  constexpr char kBackground2[] =
      R"(
    chrome.omnibox.onInputChanged.addListener((text, suggest) => {
      suggest([
        {content: 'third', description: 'extension2'},
        {content: 'fourth', description: 'extension2'}
      ]);
    });)";

  TestExtensionDir test_dir;
  TestExtensionDir test_dir2;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  test_dir2.WriteManifest(kManifest2);
  test_dir2.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground2);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  const Extension* extension2 = LoadExtension(test_dir2.UnpackedPath());
  ASSERT_TRUE(extension2);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  chrome::FocusLocationBar(browser());

  // Test that our extension can send suggestions back to us.
  AutocompleteInput input(u"", metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile()));
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  autocomplete_controller->Start(input);
  WaitForAutocompleteDone();
  EXPECT_TRUE(autocomplete_controller->done());

  const AutocompleteResult& result = autocomplete_controller->result();
  // Check if the suggestion is received (+1 for the IPH).
  ASSERT_EQ(5U, result.size()) << AutocompleteResultAsString(result);

  // Suggestions from the same extension should be grouped together and their
  // group id header should match the extension name.
  std::set<std::u16string> extension_names = {u"alpha", u"dog"};
  std::set<omnibox::GroupId> extension_group_ids = {
      omnibox::GROUP_UNSCOPED_EXTENSION_1, omnibox::GROUP_UNSCOPED_EXTENSION_2};
  {
    EXPECT_THAT(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
                testing::Eq(result.match_at(0).provider->type()));
    EXPECT_THAT(extension_group_ids,
                testing::Contains(result.match_at(0).suggestion_group_id));
    EXPECT_THAT(extension_names,
                testing::Contains(result.GetHeaderForSuggestionGroup(
                    result.match_at(0).suggestion_group_id.value())));

    EXPECT_THAT(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
                testing::Eq(result.match_at(1).provider->type()));
    EXPECT_THAT(extension_group_ids,
                testing::Contains(result.match_at(1).suggestion_group_id));
    EXPECT_THAT(extension_names,
                testing::Contains(result.GetHeaderForSuggestionGroup(
                    result.match_at(1).suggestion_group_id.value())));
  }

  extension_group_ids.erase(result.match_at(1).suggestion_group_id.value());
  extension_names.erase(result.GetHeaderForSuggestionGroup(
      result.match_at(1).suggestion_group_id.value()));

  {
    EXPECT_THAT(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
                testing::Eq(result.match_at(2).provider->type()));
    EXPECT_THAT(extension_group_ids,
                testing::Contains(result.match_at(2).suggestion_group_id));
    EXPECT_THAT(extension_names,
                testing::Contains(result.GetHeaderForSuggestionGroup(
                    result.match_at(2).suggestion_group_id.value())));

    EXPECT_THAT(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION,
                testing::Eq(result.match_at(3).provider->type()));
    EXPECT_THAT(extension_group_ids,
                testing::Contains(result.match_at(3).suggestion_group_id));
    EXPECT_THAT(extension_names,
                testing::Contains(result.GetHeaderForSuggestionGroup(
                    result.match_at(3).suggestion_group_id.value())));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace extensions
