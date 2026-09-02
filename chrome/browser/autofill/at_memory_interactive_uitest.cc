// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/at_memory/at_memory_query_service_factory.h"
#include "chrome/browser/autofill/autofill_flow_test_util.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views_test_api.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

enum class TargetElementType {
  kInputText,
  kInputNumber,
  kTextArea,
  kContentEditable,
};

std::string ToString(TargetElementType type) {
  switch (type) {
    case TargetElementType::kInputText:
      return "Input";
    case TargetElementType::kInputNumber:
      return "InputNumber";
    case TargetElementType::kTextArea:
      return "TextArea";
    case TargetElementType::kContentEditable:
      return "ContentEditable";
  }
}

ElementExpr GetElementById(std::string_view id) {
  return ElementExpr(
      base::StringPrintf(R"(document.getElementById('%s'))", id.data()));
}

class AtMemoryInteractiveUiTest : public AutofillUiTest,
                                  public WithParamInterface<TargetElementType> {
 public:
  AtMemoryInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kAutofillKeydownEditableElement,
                              features::kAutofillAtMemory,
                              features::kAutofillAtMemoryDoubleCtrl,
                              features::kAutofillAtMemoryTriggerShortcut,
                              features::kAutofillAtMemoryTriggerString,
                              features::debug::kAtMemorySkipEnablementChecks},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AtMemoryInteractiveUiTest::HandleTestURL, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();

    AtMemoryQueryServiceFactory::GetInstance()->SetTestingFactory(
        browser()->GetProfile(),
        base::BindRepeating(
            &AtMemoryInteractiveUiTest::CreateMockAtMemoryQueryService,
            base::Unretained(this)));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleTestURL(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/test.html") {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html;charset=utf-8");
    response->set_content(GetHtmlContent(GetParam()));
    return response;
  }

  std::unique_ptr<KeyedService> CreateMockAtMemoryQueryService(
      content::BrowserContext* context) {
    auto mock_service = std::make_unique<NiceMock<MockAtMemoryQueryService>>();
    ON_CALL(*mock_service, Query)
        .WillByDefault(
            [type = GetParam()](
                std::u16string_view query, const GURL& url,
                std::u16string_view title,
                base::RepeatingCallback<void(MemorySearchResults)> callback) {
              MemorySearchResult entry(MemoryDataType::kAddressStreetAddress,
                                       u"Result", GetFilledValue(type), 1.0);
              entry.sources = {
                  MemoryEntrySource(MemoryEntrySourceType::kAutofill)};
              callback.Run(
                  MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                                      {std::move(entry)}));
            });
    return mock_service;
  }

  static std::u16string GetFilledValue(TargetElementType type) {
    switch (type) {
      case TargetElementType::kInputText:
        return u"Donald Trump";
      case TargetElementType::kInputNumber:
        return u"1961";
      case TargetElementType::kTextArea:
      case TargetElementType::kContentEditable:
        return u"1600 Pennsylvania Avenue NW\nWashington, DC 20500";
    }
    NOTREACHED();
  }

  static std::string GetHtmlContent(TargetElementType type) {
    switch (type) {
      case TargetElementType::kInputText:
        return "<input type=text id=target value=FooBar>";
      case TargetElementType::kInputNumber:
        return "<input type=number id=target>";
      case TargetElementType::kTextArea:
        return "<textarea id=target>FooBar</textarea>";
      case TargetElementType::kContentEditable:
        return "<div id=target contenteditable>FooBar</div>";
    }
    NOTREACHED();
  }

  static std::string GetExpectedValue(TargetElementType type) {
    switch (type) {
      case TargetElementType::kInputText:
      case TargetElementType::kTextArea:
      case TargetElementType::kContentEditable:
        return "Foo" + base::UTF16ToUTF8(GetFilledValue(type)) + "Bar";
      case TargetElementType::kInputNumber:
        return base::UTF16ToUTF8(GetFilledValue(type));
    }
    NOTREACHED();
  }

  std::string GetTargetValue(TargetElementType type) {
    switch (type) {
      case TargetElementType::kInputText:
      case TargetElementType::kInputNumber:
      case TargetElementType::kTextArea:
        return content::EvalJs(GetWebContents(),
                               R"(document.getElementById('target').value)")
            .ExtractString();
      case TargetElementType::kContentEditable:
        return content::EvalJs(GetWebContents(),
                               R"(document.getElementById('target').innerText)")
            .ExtractString();
    }
    NOTREACHED();
  }

  // Waits until the target element's value matches `expected_value`.
  bool WaitForTargetValue(std::string_view expected_value) {
    return base::test::RunUntil(
        [&] { return GetTargetValue(GetParam()) == expected_value; });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         AtMemoryInteractiveUiTest,
                         Values(TargetElementType::kInputText,
                                TargetElementType::kInputNumber,
                                TargetElementType::kTextArea,
                                TargetElementType::kContentEditable),
                         [](const TestParamInfo<TargetElementType>& info) {
                           return ToString(info.param);
                         });

// Tests that typing the trigger string "@@" in various target elements
// (input, number input, textarea, contenteditable) opens the AtMemory popup,
// allows searching, and replaces the trigger string with the selected value
// upon suggestion acceptance.
IN_PROC_BROWSER_TEST_P(AtMemoryInteractiveUiTest, TriggerAndFill) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/test.html")));

  // Focus the field and move the caret between "Foo" and "Bar".
  GetWebContents()->Focus();
  ASSERT_TRUE(FocusField(GetElementById("target"), GetWebContents()));
  ASSERT_TRUE(content::ExecJs(GetWebContents(),
                              R"(const el = document.getElementById('target');
         if (el.setSelectionRange && el.type !== 'number') {
           el.setSelectionRange(3, 3);
         } else if (window.getSelection && el.isContentEditable) {
           const range = document.createRange();
           range.setStart(el.childNodes[0], 3);
           range.collapse(true);
           const sel = window.getSelection();
           sel.removeAllRanges();
           sel.addRange(range);
         })"));
  // Wait for the selection to be processed.
  content::RunUntilInputProcessed(
      GetWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost());

  // The input events below will not be processed until the end of the current
  // paint, so we need to wait for that to happen before sending the key events.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(GetWebContents());

  // Type '@'.
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::FromCharacter('@'),
                                   ui::DomCode::DIGIT2, ui::VKEY_2, {}));

  // Type second '@', completing trigger string and showing suggestions.
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::FromCharacter('@'),
                                   ui::DomCode::DIGIT2, ui::VKEY_2,
                                   {ObservedUiEvents::kSuggestionsShown}));

  // Set search query in the popup search bar.
  base::WeakPtr<AutofillSuggestionController> controller =
      ChromeAutofillClient::FromWebContentsForTesting(GetWebContents())
          ->suggestion_controller_for_testing();
  ASSERT_TRUE(controller);
  base::WeakPtr<AutofillPopupView> view =
      test_api(static_cast<AutofillPopupControllerImpl&>(*controller)).view();
  ASSERT_TRUE(view);
  test_api(static_cast<PopupViewViews&>(*view)).SetSearchQuery(u"address");

  // Submit the search bar query and wait for search results suggestions.
  ASSERT_TRUE(SendKeyToPopupAndWait(ui::DomKey::ENTER,
                                    {ObservedUiEvents::kSuggestionsShown}));

  // Disable threshold so suggestion can be accepted immediately.
  if (base::WeakPtr<AutofillSuggestionController> active_controller =
          ChromeAutofillClient::FromWebContentsForTesting(GetWebContents())
              ->suggestion_controller_for_testing()) {
    test_api(static_cast<AutofillPopupControllerImpl&>(*active_controller))
        .DisableThreshold(true);
  }

  // Select the first search result suggestion.
  SendKeyToPopup(GetWebContents()->GetPrimaryMainFrame(),
                 ui::DomKey::ARROW_DOWN);

  // Accept the suggestion and wait for popup to hide.
  ASSERT_TRUE(SendKeyToPopupAndWait(ui::DomKey::ENTER,
                                    {ObservedUiEvents::kSuggestionsHidden}));

  // Verify the trigger string "@@" is replaced with the filled value.
  EXPECT_TRUE(WaitForTargetValue(GetExpectedValue(GetParam())));
}

}  // namespace
}  // namespace autofill
