// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_TEST_UTILS_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/actor_form_filling_service_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class TestActorContentAutofillDriver : public TestContentAutofillDriver {
 public:
  TestActorContentAutofillDriver(content::RenderFrameHost* rfh,
                                 ContentAutofillDriverFactory* factory);
  ~TestActorContentAutofillDriver() override;

  MOCK_METHOD(void, RendererShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void, ScrollFieldIntoView, (FieldGlobalId), (override));

  MOCK_METHOD(
      base::flat_set<FieldGlobalId>,
      ApplyFormAction,
      (mojom::FormActionType action_type,
       mojom::ActionPersistence action_persistence,
       base::span<const FormFieldData> fields,
       const FillId& fill_id,
       bool supports_refill,
       const url::Origin& triggered_origin,
       (const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map),
       const Section& section_for_clear_form_on_ios),
      (override));

  base::flat_set<FieldGlobalId> BaseApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData> fields,
      const FillId& fill_id,
      bool supports_refill,
      const url::Origin& triggered_origin,
      const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map,
      const Section& section_for_clear_form_on_ios);
};

class TestCreditCardAccessManager : public CreditCardAccessManager {
 public:
  explicit TestCreditCardAccessManager(BrowserAutofillManager* manager);
  ~TestCreditCardAccessManager() override;

  void PrepareToFetchCreditCard() override {}

  void FetchCreditCard(const CreditCard*,
                       OnCreditCardFetchedCallback callback) override;

  [[nodiscard]] bool RunCreditCardFetchedCallback(const CreditCard& card);

 private:
  OnCreditCardFetchedCallback callback_;
};

class TestBrowserAutofillManagerWithTestCCAM
    : public TestBrowserAutofillManager {
 public:
  explicit TestBrowserAutofillManagerWithTestCCAM(AutofillDriver* driver);
  ~TestBrowserAutofillManagerWithTestCCAM() override;

  void Reset() override;

  void FillOrPreviewForm(
      mojom::ActionPersistence action_persistence,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const FillingPayload& filling_payload,
      AutofillTriggerSource trigger_source,
      const base::flat_set<FieldGlobalId>& blocked_fields) override;

  FieldGlobalId last_trigger_field_id() const { return last_trigger_field_id_; }

 private:
  FieldGlobalId last_trigger_field_id_;
};

class TestActorChromeAutofillClient : public TestContentAutofillClient {
 public:
  explicit TestActorChromeAutofillClient(content::WebContents* web_contents);
  ~TestActorChromeAutofillClient() override;

  std::unique_ptr<AutofillManager> CreateManager(
      base::PassKey<ContentAutofillDriver> pass_key,
      ContentAutofillDriver& driver) override;

  ActorKeyMetricsRecorder* GetActorKeyMetricsRecorder() override;

 private:
  std::unique_ptr<ActorKeyMetricsRecorder> recorder_;
};

class ActorTestBase : public ChromeRenderViewHostTestHarness {
 public:
  ActorTestBase();
  ~ActorTestBase() override;

  void SetUp() override;

  FormData SeeForm(test::FormDescription form_description);

  const absl::flat_hash_map<FieldGlobalId, std::u16string>& last_filled_values()
      const {
    return last_filled_values_;
  }

 protected:
  TestActorChromeAutofillClient& client();
  TestCreditCardAccessManager& credit_card_access_manager();
  PaymentsDataManager& payments_data_manager();
  TestActorContentAutofillDriver& driver();
  TestBrowserAutofillManagerWithTestCCAM& manager();
  ActorFormFillingServiceImpl& service() { return *service_; }
  tabs::TabInterface& tab() { return mock_tab; }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  tabs::MockTabInterface mock_tab;
  TestAutofillClientInjector<TestActorChromeAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<TestActorContentAutofillDriver>
      autofill_driver_injector_;
  ::actor::AggregatedJournal journal_;
  std::unique_ptr<ActorFormFillingServiceImpl> service_;
  absl::flat_hash_map<FieldGlobalId, std::u16string> last_filled_values_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_TEST_UTILS_H_
