// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

// Adds waiting capabilities to BrowserAutofillManager.
class TestAutofillManager : public autofill::BrowserAutofillManager {
 public:
  TestAutofillManager(autofill::ContentAutofillDriver* driver,
                      autofill::AutofillClient* client)
      : BrowserAutofillManager(driver, client, "en-US") {}

  static TestAutofillManager* GetForRenderFrameHost(
      content::RenderFrameHost* rfh) {
    return static_cast<TestAutofillManager*>(
        autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh)
            ->autofill_manager());
  }

  [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
      size_t num_awaited_calls) {
    return forms_seen_.Wait(num_awaited_calls);
  }

 private:
  autofill::TestAutofillManagerWaiter forms_seen_{
      *this,
      {autofill::AutofillManagerEvent::kFormsSeen}};
};

class DevToolsAutofillTest : public DevToolsProtocolTestBase {
 public:
  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }
  TestAutofillManager* main_autofill_manager() {
    return TestAutofillManager::GetForRenderFrameHost(main_frame());
  }

 private:
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;
};

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, TriggerCreditCard) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/autofill_creditcard_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  content::SimulateMouseClickOrTapElementWithId(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "CREDIT_CARD_NUMBER");

  EXPECT_TRUE(main_autofill_manager()->WaitForFormsSeen(1));

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

  std::string object_id;
  {
    base::Value::Dict params;
    params.Set("expression", "document.getElementById('CREDIT_CARD_NUMBER')");
    const base::Value::Dict* result =
        SendCommand("Runtime.evaluate", std::move(params));
    object_id = *result->FindStringByDottedPath("result.objectId");
  }

  int backend_node_id = 0;
  {
    base::Value::Dict params;
    params.Set("objectId", object_id);
    const base::Value::Dict* result =
        SendCommand("DOM.describeNode", std::move(params));
    backend_node_id = *result->FindIntByDottedPath("node.backendNodeId");
  }

  base::Value::Dict params;
  params.Set("fieldId", backend_node_id);

  base::Value::Dict card;
  card.Set("number", "4444444444444444");
  card.Set("name", "John Smith");
  card.Set("expiryMonth", "01");
  card.Set("expiryYear", "2030");
  card.Set("cvc", "123");
  params.Set("card", std::move(card));

  const base::Value::Dict* result =
      SendCommandSync("Autofill.trigger", std::move(params));

  EXPECT_EQ(*result, base::Value::Dict());

  std::string ccNameResult =
      EvalJs(rfh, "document.getElementById('CREDIT_CARD_NAME_FULL').value")
          .ExtractString();
  std::string ccNumberResult =
      EvalJs(rfh, "document.getElementById('CREDIT_CARD_NUMBER').value")
          .ExtractString();
  std::string ccExpiryMonthResult =
      EvalJs(rfh, "document.getElementById('CREDIT_CARD_EXP_MONTH').value")
          .ExtractString();
  std::string ccExpiryYearResult =
      EvalJs(rfh,
             "document.getElementById('CREDIT_CARD_EXP_4_DIGIT_YEAR').value")
          .ExtractString();
  EXPECT_EQ(ccNameResult, "John Smith");
  EXPECT_EQ(ccNumberResult, "4444444444444444");
  EXPECT_EQ(ccExpiryMonthResult, "01");
  EXPECT_EQ(ccExpiryYearResult, "2030");
}

}  // namespace
