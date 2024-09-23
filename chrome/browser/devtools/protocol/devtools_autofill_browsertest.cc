// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Eq;
using testing::Not;
using testing::ResultOf;

namespace autofill {
namespace {

// Asserts that a filled field sent to devtools has `attribute` set with
// `expected_value`.
auto FilledFieldHasAttributeWithValue(const std::string& attribute,
                                      const std::string& expected_value) {
  return ResultOf(
      [&](const base::Value& filled_field) {
        const std::string* value =
            filled_field.GetDict().FindStringByDottedPath(attribute);
        return value ? *value : "";
      },
      Eq(expected_value));
}

// Returns the all the `autofill::FieldType`s used to build an UI representation
// of an address. These values are the same as the ones used in the settings
// page and edit dialog, furthermore, they depend on a profile's `country_code`.
std::set<autofill::FieldType>
GetExpectedFieldTypesToBuildAddressUiForCountryCode(
    const std::string& country_code) {
  std::set<autofill::FieldType> expected_address_ui_field_types;

  // These types are not part of `AutofillAddressUIComponent`.
  expected_address_ui_field_types.insert(
      autofill::FieldType::ADDRESS_HOME_COUNTRY);
  expected_address_ui_field_types.insert(
      autofill::FieldType::PHONE_HOME_WHOLE_NUMBER);
  expected_address_ui_field_types.insert(autofill::FieldType::EMAIL_ADDRESS);

  std::vector<std::vector<autofill::AutofillAddressUIComponent>> components;
  autofill::GetAddressComponents(country_code, "en-US",
                                 /*include_literals=*/false, &components,
                                 nullptr);
  for (const std::vector<autofill::AutofillAddressUIComponent>& line :
       components) {
    for (const autofill::AutofillAddressUIComponent& component : line) {
      expected_address_ui_field_types.insert(component.field);
    }
  }

  return expected_address_ui_field_types;
}

auto FilledFieldHasAttributeWithValue16(const std::string& attribute,
                                        const std::u16string& expected_value) {
  return FilledFieldHasAttributeWithValue(attribute,
                                          base::UTF16ToUTF8(expected_value));
}

std::string GetProfileInfoFromAddressField(const AutofillProfile profile,
                                           const base::Value& address_field) {
  return base::UTF16ToUTF8(profile.GetInfo(
      TypeNameToFieldType(
          *address_field.GetDict().FindStringByDottedPath("name")),
      "en-US"));
}

// Adds waiting capabilities to BrowserAutofillManager.
class TestAutofillManager : public autofill::BrowserAutofillManager {
 public:
  explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver, "en-US") {}

  static TestAutofillManager& GetForRenderFrameHost(
      content::RenderFrameHost* rfh) {
    return static_cast<TestAutofillManager&>(
        autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh)
            ->GetAutofillManager());
  }

  [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
      size_t num_awaited_calls) {
    return forms_seen_.Wait(num_awaited_calls);
  }

  const FormStructure* WaitForFormWithNFields(int n) {
    return WaitForMatchingForm(this, base::BindRepeating(
                                         [](int n, const FormStructure& form) {
                                           return form.active_field_count() ==
                                                  (size_t)n;
                                         },
                                         n));
  }

 private:
  autofill::TestAutofillManagerWaiter forms_seen_{
      *this,
      {autofill::AutofillManagerEvent::kFormsSeen}};
};

class DevToolsAutofillTest : public DevToolsProtocolTestBase {
 public:
  DevToolsAutofillTest() {
    feature_list_.InitWithFeatures(
        {features::kAutofillTestFormWithTestAddresses},
        {});
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestAutofillManager& main_autofill_manager() {
    return TestAutofillManager::GetForRenderFrameHost(main_frame());
  }

  std::string EvaluateAndGetValue(const std::string& expression,
                                  const std::string& unique_context_id,
                                  const std::string& session_id) {
    base::Value::Dict params;
    params.Set("expression", expression);
    if (!unique_context_id.empty()) {
      params.Set("uniqueContextId", unique_context_id);
    }
    const base::Value::Dict* result = SendSessionCommand(
        "Runtime.evaluate", std::move(params), session_id, /*wait=*/true);
    return *result->FindStringByDottedPath("result.value");
  }

  int GetBackendNodeIdByIdAttribute(const std::string& id_attribute) {
    return GetBackendNodeIdByIdAttribute(id_attribute, "", "");
  }

  int GetBackendNodeIdByIdAttribute(const std::string& id_attribute,
                                    const std::string& unique_context_id) {
    return GetBackendNodeIdByIdAttribute(id_attribute, unique_context_id, "");
  }

  int GetBackendNodeIdByIdAttribute(const std::string& id_attribute,
                                    const std::string& unique_context_id,
                                    const std::string& session_id) {
    std::string object_id;
    {
      base::Value::Dict params;
      params.Set("expression", base::StrCat({"document.getElementById('",
                                             id_attribute, "')"}));
      if (!unique_context_id.empty()) {
        params.Set("uniqueContextId", unique_context_id);
      }
      const base::Value::Dict* result = SendSessionCommand(
          "Runtime.evaluate", std::move(params), session_id, /*wait=*/true);
      object_id = *result->FindStringByDottedPath("result.objectId");
    }

    base::Value::Dict params;
    params.Set("objectId", object_id);
    const base::Value::Dict* result = SendSessionCommand(
        "DOM.describeNode", std::move(params), session_id, /*wait=*/true);
    return *result->FindIntByDottedPath("node.backendNodeId");
  }

  std::string GetOOPIFTargetId() {
    base::Value::Dict params;
    params.Set("discover", true);
    SendCommandSync("Target.setDiscoverTargets", std::move(params));
    std::string frame_target_id;
    while (true) {
      base::Value::Dict result;
      result = WaitForNotification("Target.targetCreated", true);
      if (*result.FindStringByDottedPath("targetInfo.type") == "iframe") {
        frame_target_id =
            std::string(*result.FindStringByDottedPath("targetInfo.targetId"));
        break;
      }
    }
    return frame_target_id;
  }

  std::string AttachToTarget(const std::string& target_id) {
    base::Value::Dict params;
    params.Set("targetId", target_id);
    params.Set("flatten", true);
    SendCommandSync("Target.attachToTarget", std::move(params));
    return CHECK_DEREF(CHECK_DEREF(result()).FindString("sessionId"));
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

  AutofillProfile CreateTestProfile() {
    AutofillProfile profile = test::GetFullProfile();
    AddTestProfile(browser()->profile(), profile);
    return profile;
  }

  FormGlobalId form_id() const { return form_id_; }

  base::Value::Dict GetFilledOutForm(const std::string& unique_context_id) {
    return GetFilledOutForm(unique_context_id, "");
  }

  base::Value::Dict GetFilledOutForm(const std::string& unique_context_id,
                                     const std::string& session_id) {
    base::Value::Dict card;
    card.Set("number",
             EvaluateAndGetValue(
                 "document.getElementById('CREDIT_CARD_NUMBER').value",
                 unique_context_id, session_id));
    card.Set("name",
             EvaluateAndGetValue(
                 "document.getElementById('CREDIT_CARD_NAME_FULL').value",
                 unique_context_id, session_id));
    card.Set("expiryMonth",
             EvaluateAndGetValue(
                 "document.getElementById('CREDIT_CARD_EXP_MONTH').value",
                 unique_context_id, session_id));
    card.Set(
        "expiryYear",
        EvaluateAndGetValue(
            "document.getElementById('CREDIT_CARD_EXP_4_DIGIT_YEAR').value",
            unique_context_id, session_id));
    // CVC is not filled out in the form.
    card.Set("cvc", "123");
    return card;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
  FormGlobalId form_id_ = test::MakeFormGlobalId();
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;
};

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, SetAddresses) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/autofill_creditcard_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  EXPECT_TRUE(main_autofill_manager().WaitForFormsSeen(1));

  base::Value::Dict address_1_fields;
  address_1_fields.Set("name", "ADDRESS_HOME_LINE1");
  address_1_fields.Set("value", "Erika-mann");
  base::Value::Dict address_1;
  base::Value::List fields;
  fields.Append(std::move(address_1_fields));
  address_1.Set("fields", std::move(fields));

  base::Value::Dict address_2_fields;
  address_2_fields.Set("name", "ADDRESS_HOME_LINE2");
  address_2_fields.Set("value", "Faria lima");
  base::Value::Dict address_2;
  base::Value::List fields_2;
  fields_2.Append(std::move(address_2_fields));
  address_2.Set("fields", std::move(fields_2));

  base::Value::List test_addresses;
  test_addresses.Append(std::move(address_1));
  test_addresses.Append(std::move(address_2));

  base::Value::Dict params;
  params.Set("addresses", std::move(test_addresses));

  SendCommandSync("Autofill.setAddresses", std::move(params));

  base::span<const autofill::AutofillProfile> res =
      main_autofill_manager().client().GetTestAddresses();
  ASSERT_EQ(res.size(), 2u);
  ASSERT_EQ(
      res[0].GetAddress().GetRawInfo(autofill::FieldType::ADDRESS_HOME_LINE1),
      u"Erika-mann");
  ASSERT_EQ(
      res[1].GetAddress().GetRawInfo(autofill::FieldType::ADDRESS_HOME_LINE2),
      u"Faria lima");
}

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, TriggerCreditCard) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/autofill_creditcard_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  EXPECT_TRUE(main_autofill_manager().WaitForFormsSeen(1));

  int backend_node_id = GetBackendNodeIdByIdAttribute("CREDIT_CARD_NUMBER");

  base::Value::Dict params;
  params.Set("fieldId", backend_node_id);
  params.Set("card", GetTestCreditCard());

  ASSERT_TRUE(result());
  SendCommandSync("Autofill.trigger", std::move(params));
  EXPECT_EQ(*result(), base::Value::Dict());
  EXPECT_EQ(GetFilledOutForm(""), GetTestCreditCard());
}

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, TriggerCreditCardInIframe) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "/autofill_creditcard_form_in_iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  EXPECT_TRUE(main_autofill_manager().WaitForFormsSeen(1));

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
    ASSERT_TRUE(result());
    EXPECT_EQ(*result(), base::Value::Dict());
  }

  EXPECT_EQ(GetFilledOutForm(unique_context_id), GetTestCreditCard());
}

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, TriggerCreditCardInOOPIFIframe) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill_creditcard_form_in_oopif.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  EXPECT_TRUE(main_autofill_manager().WaitForFormWithNFields(4));

  std::string frame_target_id = GetOOPIFTargetId();
  std::string session_id = AttachToTarget(frame_target_id);

  int backend_node_id =
      GetBackendNodeIdByIdAttribute("CREDIT_CARD_NUMBER", "", session_id);

  base::Value::Dict params;
  params.Set("fieldId", backend_node_id);
  params.Set("frameId", frame_target_id);
  params.Set("card", GetTestCreditCard());
  SendSessionCommand("Autofill.trigger", std::move(params), session_id,
                     /*wait=*/true);
  ASSERT_TRUE(result());
  EXPECT_EQ(*result(), base::Value::Dict());
  EXPECT_EQ(GetFilledOutForm("", session_id), GetTestCreditCard());
}

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, TriggerCreditCardAcrossOOPIFs) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill_creditcard_form_in_oopif.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  EXPECT_TRUE(main_autofill_manager().WaitForFormWithNFields(4));

  std::string frame_target_id = GetOOPIFTargetId();
  std::string session_id = AttachToTarget(frame_target_id);

  int backend_node_id =
      GetBackendNodeIdByIdAttribute("CREDIT_CARD_NUMBER", "", session_id);

  base::Value::Dict params;
  params.Set("fieldId", backend_node_id);
  params.Set("frameId", frame_target_id);
  params.Set("card", GetTestCreditCard());
  SendSessionCommand("Autofill.trigger", std::move(params), session_id,
                     /*wait=*/true);
  ASSERT_TRUE(result());
  EXPECT_EQ(*result(), base::Value::Dict());
  EXPECT_EQ(GetFilledOutForm("", session_id), GetTestCreditCard());
}

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, AddressFormFilled) {
  Attach();
  // Create a profile to read information from and send to devtools.
  AutofillProfile profile = CreateTestProfile();

  // TODO(crbug.com/40227496): Get rid of FormFieldData.
  FormData form;
  form.set_host_frame(LocalFrameToken(*main_frame()->GetFrameToken()));
  form.set_renderer_id(form_id().renderer_id);
  std::vector<FormFieldData> fields;
  fields.push_back(test::CreateTestFormField(
      /*label=*/"", "name_1", /*value=*/"", FormControlType::kInputText));
  fields.back().set_id_attribute(u"id_1");
  fields.back().set_host_frame(form.host_frame());
  fields.push_back(test::CreateTestFormField(
      /*label=*/"", "name_2", /*value=*/"", FormControlType::kInputText));
  fields.back().set_id_attribute(u"id_2");
  fields.back().set_host_frame(form.host_frame());
  form.set_fields(std::move(fields));

  // The parsed form is queried by
  // AutofillHandler::OnFillOrPreviewDataModelForm() to obtain the type
  // predictions.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->field(0)->set_server_predictions(
      {test::CreateFieldPrediction(NAME_FULL)});
  form_structure->field(0)->SetHtmlType(HtmlFieldType::kName,
                                        HtmlFieldMode::kShipping);
  form_structure->field(1)->set_server_predictions(
      {test::CreateFieldPrediction(NAME_FULL)});
  form_structure->field(1)->SetHtmlType(HtmlFieldType::kUnspecified,
                                        HtmlFieldMode::kShipping);
  (*test_api(main_autofill_manager()).mutable_form_structures())[form_id()] =
      std::move(form_structure);

  // Fake a that the fields were filled.
  test_api(form).field(0).set_value(u"value_1");
  test_api(form).field(1).set_value(u"value_2");
  const std::vector<const FormFieldData*> filled_fields_by_autofill = {
      {&form.fields()[0], &form.fields()[1]}};

  // Enable events and emit event about form being filled.
  SendCommandSync("Autofill.enable");
  main_autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnFillOrPreviewDataModelForm,
      form_id(), autofill::mojom::ActionPersistence::kFill,
      filled_fields_by_autofill, &profile);

  base::Value::Dict notification = WaitForNotification(
      "Autofill.addressFormFilled", /*allow_existing=*/true);

  std::set<autofill::FieldType> field_types_added_to_address_ui;
  for (const base::Value& address_line :
       *notification.FindListByDottedPath("addressUi.addressFields")) {
    for (const base::Value& address_field :
         *address_line.GetDict().FindListByDottedPath("fields")) {
      // Test that the profile address values sent to devtools match what we
      // have in `profile`.
      EXPECT_EQ(GetProfileInfoFromAddressField(profile, address_field),
                *address_field.GetDict().FindStringByDottedPath("value"));
      field_types_added_to_address_ui.insert(TypeNameToFieldType(
          *address_field.GetDict().FindStringByDottedPath("name")));
    }
  }

  // Assert the expected values used to build the address ui were sent to
  // devtools.
  ASSERT_EQ(field_types_added_to_address_ui,
            GetExpectedFieldTypesToBuildAddressUiForCountryCode(
                base::UTF16ToUTF8(profile.GetInfo(
                    autofill::FieldType::ADDRESS_HOME_COUNTRY, "en-US"))));
  // Assert that the filled fields sent to devtools match exactly the ones
  // filled by autofill.
  const base::Value::List* filled_fields =
      notification.FindListByDottedPath("filledFields");
  ASSERT_EQ(filled_fields->size(), filled_fields_by_autofill.size());
  for (size_t i = 0; i < filled_fields->size(); ++i) {
    FormStructure& fs =
        CHECK_DEREF(main_autofill_manager().FindCachedFormById(form_id()));
    const base::Value& ff = (*filled_fields)[i];
    const FormFieldData* ffd = filled_fields_by_autofill[i];
    const AutofillField* af = fs.GetFieldById(ffd->global_id());

    EXPECT_THAT(ff,
                FilledFieldHasAttributeWithValue16("id", af->id_attribute()));
    EXPECT_THAT(ff, FilledFieldHasAttributeWithValue(
                        "autofillType",
                        std::string(FieldTypeToDeveloperRepresentationString(
                            af->Type().GetStorableType()))));
    // Note: we read the value from `FormFieldData`.
    EXPECT_THAT(ff, FilledFieldHasAttributeWithValue16("value", ffd->value()));
    EXPECT_THAT(ff, FilledFieldHasAttributeWithValue16(
                        "frameId",
                        base::UTF8ToUTF16(
                            main_frame()->GetDevToolsFrameToken().ToString())));
    EXPECT_THAT(ff,
                Not(FilledFieldHasAttributeWithValue16(
                    "value", af->value(autofill::ValueSemantics::kCurrent))));
    EXPECT_THAT(ff,
                FilledFieldHasAttributeWithValue(
                    "htmlType", std::string(autofill::FormControlTypeToString(
                                    af->form_control_type()))));
    EXPECT_THAT(
        ff, FilledFieldHasAttributeWithValue16("name", af->name_attribute()));
    EXPECT_EQ(*ff.GetDict().FindIntByDottedPath("fieldId"),
              (int)(ffd->renderer_id().value()));
  }

  // The first filled field uses autocomplete attribute as filling strategy.
  EXPECT_EQ(*filled_fields->front().GetDict().FindStringByDottedPath(
                "fillingStrategy"),
            "autocompleteAttribute");
  // The second one used autofill internals, either local heuristics or server
  // predictions.
  EXPECT_EQ(*filled_fields->back().GetDict().FindStringByDottedPath(
                "fillingStrategy"),
            "autofillInferred");
}

// This test guards the assumption that autofill events in iframes are routed to
// the main frame on the browser side. It's implicitly assumed in tests that
// make direct `AutofillManager::Observer` calls on `main_autofill_manager()`
// while testing iframes.
IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, AutofillInOOPIFs) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill_address_multi_form_in_oopif.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_TRUE(main_autofill_manager().WaitForFormWithNFields(10));
  ASSERT_EQ(main_autofill_manager().form_structures().size(), 1u);

  FormData form =
      main_autofill_manager().form_structures().begin()->second->ToFormData();

  autofill::MockAutofillManagerObserver observer;
  main_autofill_manager().AddObserver(&observer);

  // Expect the `AskForValuesToFill()` call below to be routed to the main
  // frame `AutofillManager`.
  EXPECT_CALL(observer,
              OnBeforeAskForValuesToFill(_, form.global_id(),
                                         form.fields()[0].global_id(), _));

  const std::vector<const FormFieldData*> filled_fields_by_autofill = {
      {&form.fields()[0], &form.fields()[1]}};
  web_contents()->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    // Call the driver of the field host iframe.
    if (rfh->GetFrameToken().ToString() ==
        form.fields()[0].host_frame()->ToString()) {
      ASSERT_NE(rfh->GetFrameToken(), main_frame()->GetFrameToken());
      auto* driver = static_cast<mojom::AutofillDriver*>(
          autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh));
      driver->AskForValuesToFill(
          form, form.fields()[0].renderer_id(), gfx::Rect(0, 10),
          ::autofill::mojom::AutofillSuggestionTriggerSource::kUnspecified);
    }
  });

  main_autofill_manager().RemoveObserver(&observer);
}

// Tests that only the handler associated with the root frame (page) handles
// AutofillManager events (for cross-iframe filling, the filling events from all
// frames are routed to the root AutofillManager and others don't emit them).
// It also tests that only relevant elements (those from forms which have at
// least one field autofilled, others are ignored) are sent to the frontend.
IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, AddressFormFilledInOOPIFs) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill_address_multi_form_in_oopif.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  Attach();

  EXPECT_TRUE(main_autofill_manager().WaitForFormWithNFields(10));
  ASSERT_EQ(main_autofill_manager().form_structures().size(), 1u);

  std::string frame_target_id = GetOOPIFTargetId();
  std::string session_id = AttachToTarget(frame_target_id);

  SendCommandSync("Autofill.enable");
  SendSessionCommand("Autofill.enable", base::Value::Dict(), session_id, true);

  AutofillProfile profile = CreateTestProfile();
  FormData form =
      main_autofill_manager().form_structures().begin()->second->ToFormData();
  const std::vector<const FormFieldData*> filled_fields_by_autofill = {
      {&form.fields()[0], &form.fields()[1]}};
  main_autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnFillOrPreviewDataModelForm,
      form.global_id(), autofill::mojom::ActionPersistence::kFill,
      filled_fields_by_autofill, &profile);

  base::Value::Dict notification = WaitForNotification(
      "Autofill.addressFormFilled", /*allow_existing=*/true);
  EXPECT_EQ(notification.FindListByDottedPath("filledFields")->size(), 6u);
  EXPECT_FALSE(HasExistingNotification("Autofill.addressFormFilled"))
      << "The other handler should not handle `OnFillOrPreviewDataModelForm`";
}

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest,
                       AutofillManagerEventsAfterNavigation) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("a.com", "/autofill_test_form.html")));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_TRUE(main_autofill_manager().WaitForFormWithNFields(9));

  Attach();
  SendCommandSync("Autofill.enable");

  AutofillProfile profile_a = CreateTestProfile();
  FormData form_a =
      main_autofill_manager().form_structures().begin()->second->ToFormData();
  const std::vector<const FormFieldData*> filled_fields_by_autofill_a = {
      {&form_a.fields()[0], &form_a.fields()[1]}};
  main_autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnFillOrPreviewDataModelForm,
      form_a.global_id(), autofill::mojom::ActionPersistence::kFill,
      filled_fields_by_autofill_a, &profile_a);

  WaitForNotification("Autofill.addressFormFilled", /*allow_existing=*/true);

  // Navigating from "a.com" to "b.com".
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("b.com", "/autofill_test_form.html")));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_TRUE(main_autofill_manager().WaitForFormWithNFields(9));

  AutofillProfile profile_b = CreateTestProfile();
  FormData form_b =
      main_autofill_manager().form_structures().begin()->second->ToFormData();
  const std::vector<const FormFieldData*> filled_fields_by_autofill_b = {
      {&form_b.fields()[0], &form_b.fields()[1]}};
  main_autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnFillOrPreviewDataModelForm,
      form_b.global_id(), autofill::mojom::ActionPersistence::kFill,
      filled_fields_by_autofill_b, &profile_b);
  WaitForNotification("Autofill.addressFormFilled", /*allow_existing=*/true);
}

}  // namespace
}  // namespace autofill
