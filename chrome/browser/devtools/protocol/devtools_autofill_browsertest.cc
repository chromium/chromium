// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "build/chromeos_buildflags.h"
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

  std::string EvaluateAndGetValue(const std::string& expression,
                                  const std::string& unique_context_id) {
    base::Value::Dict params;
    params.Set("expression", expression);
    if (!unique_context_id.empty()) {
      params.Set("uniqueContextId", unique_context_id);
    }
    const base::Value::Dict* result =
        SendCommand("Runtime.evaluate", std::move(params));
    return *result->FindStringByDottedPath("result.value");
  }

  int GetBackendNodeIdByIdAttribute(const std::string& expression) {
    return GetBackendNodeIdByIdAttribute(expression, "");
  }

  int GetBackendNodeIdByIdAttribute(const std::string& id_attribute,
                                    const std::string& unique_context_id) {
    std::string object_id;
    {
      base::Value::Dict params;
      params.Set("expression", base::StrCat({"document.getElementById('",
                                             id_attribute, "')"}));
      if (!unique_context_id.empty()) {
        params.Set("uniqueContextId", unique_context_id);
      }
      const base::Value::Dict* result =
          SendCommand("Runtime.evaluate", std::move(params));
      object_id = *result->FindStringByDottedPath("result.objectId");
    }

    base::Value::Dict params;
    params.Set("objectId", object_id);
    const base::Value::Dict* result =
        SendCommand("DOM.describeNode", std::move(params));
    return *result->FindIntByDottedPath("node.backendNodeId");
  }

  base::Value::Dict GetTestCreditCard() {
    base::Value::Dict card;
    card.Set("number", "4444444444444444");
    card.Set("name", "John Smith");
    card.Set("expiryMonth", "01");
    card.Set("expiryYear", "2030");
    card.Set("cvc", "123");
    return card;
  }

  base::Value::Dict GetFilledOutForm(const std::string& unique_context_id) {
    base::Value::Dict card;
    card.Set("number",
             EvaluateAndGetValue(
                 "document.getElementById('CREDIT_CARD_NUMBER').value",
                 unique_context_id));
    card.Set("name",
             EvaluateAndGetValue(
                 "document.getElementById('CREDIT_CARD_NAME_FULL').value",
                 unique_context_id));
    card.Set("expiryMonth",
             EvaluateAndGetValue(
                 "document.getElementById('CREDIT_CARD_EXP_MONTH').value",
                 unique_context_id));
    card.Set(
        "expiryYear",
        EvaluateAndGetValue(
            "document.getElementById('CREDIT_CARD_EXP_4_DIGIT_YEAR').value",
            unique_context_id));
    // CVC is not filled out in the form.
    card.Set("cvc", "123");
    return card;
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

  EXPECT_TRUE(main_autofill_manager()->WaitForFormsSeen(1));

  int backend_node_id = GetBackendNodeIdByIdAttribute("CREDIT_CARD_NUMBER");

  base::Value::Dict params;
  params.Set("fieldId", backend_node_id);
  params.Set("card", GetTestCreditCard());

  SendCommandSync("Autofill.trigger", std::move(params));
  EXPECT_EQ(*result(), base::Value::Dict());
  EXPECT_EQ(GetFilledOutForm(""), GetTestCreditCard());
}

// TODO(crbug.com/1445476): The test currently fails with Chrome-branded
// patterns in Autofill.
IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest,
                       DISABLED_TriggerCreditCardInIframe) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "/autofill_creditcard_form_in_iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  EXPECT_TRUE(main_autofill_manager()->WaitForFormsSeen(1));

  std::string frame_id;
  {
    const base::Value::Dict* result = SendCommandSync("Page.getFrameTree");
    const base::Value::List* frames =
        result->FindListByDottedPath("frameTree.childFrames");
    const base::Value::Dict* frame_dict = frames->front().GetIfDict();
    frame_id = *frame_dict->FindStringByDottedPath("frame.id");
  }

  std::string unique_context_id;
  {
    base::Value::Dict command_params;
    SendCommandSync("Runtime.enable");
    base::Value::Dict params;
    for (int context_count = 1; true; context_count++) {
      params = WaitForNotification("Runtime.executionContextCreated", true);
      if (*params.FindStringByDottedPath("context.auxData.frameId") ==
          frame_id) {
        unique_context_id = *params.FindStringByDottedPath("context.uniqueId");
        break;
      }
      ASSERT_LT(context_count, 2);
    }
  }

  int backend_node_id =
      GetBackendNodeIdByIdAttribute("CREDIT_CARD_NUMBER", unique_context_id);

  {
    base::Value::Dict params;
    params.Set("fieldId", backend_node_id);
    params.Set("card", GetTestCreditCard());
    params.Set("frameId", "wrong");
    SendCommandSync("Autofill.trigger", std::move(params));
    EXPECT_EQ(*error()->FindString("message"), "Frame not found");
  }

  {
    base::Value::Dict params;
    params.Set("fieldId", backend_node_id);
    params.Set("card", GetTestCreditCard());
    params.Set("frameId", frame_id);
    SendCommandSync("Autofill.trigger", std::move(params));
    EXPECT_EQ(*result(), base::Value::Dict());
  }

  EXPECT_EQ(GetFilledOutForm(unique_context_id), GetTestCreditCard());
}

}  // namespace
