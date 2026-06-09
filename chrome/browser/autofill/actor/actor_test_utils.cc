// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_test_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestActorContentAutofillDriver::TestActorContentAutofillDriver(
    content::RenderFrameHost* rfh,
    ContentAutofillDriverFactory* factory)
    : TestContentAutofillDriver(rfh, factory) {}

TestActorContentAutofillDriver::~TestActorContentAutofillDriver() = default;

base::flat_set<FieldGlobalId>
TestActorContentAutofillDriver::BaseApplyFormAction(
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    base::span<const FormFieldData> fields,
    const FillId& fill_id,
    bool supports_refill,
    const url::Origin& triggered_origin,
    const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map,
    const Section& section_for_clear_form_on_ios) {
  return TestContentAutofillDriver::ApplyFormAction(
      action_type, action_persistence, fields, fill_id, supports_refill,
      triggered_origin, field_type_map, section_for_clear_form_on_ios);
}

TestCreditCardAccessManager::TestCreditCardAccessManager(
    BrowserAutofillManager* manager)
    : CreditCardAccessManager(manager) {}

TestCreditCardAccessManager::~TestCreditCardAccessManager() = default;

void TestCreditCardAccessManager::FetchCreditCard(
    const CreditCard*,
    OnCreditCardFetchedCallback callback) {
  callback_ = std::move(callback);
}

bool TestCreditCardAccessManager::RunCreditCardFetchedCallback(
    const CreditCard& card) {
  if (!callback_) {
    return false;
  }
  std::move(callback_).Run(card);
  return true;
}

TestBrowserAutofillManagerWithTestCCAM::TestBrowserAutofillManagerWithTestCCAM(
    AutofillDriver* driver)
    : TestBrowserAutofillManager(driver) {
  test_api(*this).set_credit_card_access_manager(
      std::make_unique<TestCreditCardAccessManager>(this));
}

TestBrowserAutofillManagerWithTestCCAM::
    ~TestBrowserAutofillManagerWithTestCCAM() = default;

void TestBrowserAutofillManagerWithTestCCAM::Reset() {
  TestBrowserAutofillManager::Reset();
  test_api(*this).set_credit_card_access_manager(
      std::make_unique<TestCreditCardAccessManager>(this));
}

void TestBrowserAutofillManagerWithTestCCAM::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const FillingPayload& filling_payload,
    AutofillTriggerSource trigger_source,
    const base::flat_set<FieldGlobalId>& blocked_fields) {
  last_trigger_field_id_ = field_id;
  TestBrowserAutofillManager::FillOrPreviewForm(action_persistence, form_id,
                                                field_id, filling_payload,
                                                trigger_source, blocked_fields);
}

TestActorChromeAutofillClient::TestActorChromeAutofillClient(
    content::WebContents* web_contents)
    : TestContentAutofillClient(web_contents) {
  recorder_ = std::make_unique<ActorKeyMetricsRecorder>(this);
}

TestActorChromeAutofillClient::~TestActorChromeAutofillClient() = default;

std::unique_ptr<AutofillManager> TestActorChromeAutofillClient::CreateManager(
    base::PassKey<ContentAutofillDriver> pass_key,
    ContentAutofillDriver& driver) {
  return std::make_unique<TestBrowserAutofillManagerWithTestCCAM>(&driver);
}

ActorKeyMetricsRecorder*
TestActorChromeAutofillClient::GetActorKeyMetricsRecorder() {
  return recorder_.get();
}

ActorTestBase::ActorTestBase()
    : ChromeRenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      service_(
          std::make_unique<ActorFormFillingServiceImpl>(journal_.GetSafeRef(),
                                                        ::actor::TaskId(1))) {}

ActorTestBase::~ActorTestBase() = default;

void ActorTestBase::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents()));
  NavigateAndCommit(GURL("about:blank"));
  client().GetPersonalDataManager().address_data_manager().AddProfile(
      test::GetFullProfile());

  ON_CALL(driver(), ApplyFormAction)
      .WillByDefault([&](mojom::FormActionType action_type,
                         mojom::ActionPersistence action_persistence,
                         base::span<const FormFieldData> fields,
                         const FillId& fill_id, bool supports_refill,
                         const url::Origin& triggered_origin,
                         const absl::flat_hash_map<FieldGlobalId, FieldType>&
                             field_type_map,
                         const Section& section_for_clear_form_on_ios) {
        base::flat_set<FieldGlobalId> filled_fields =
            driver().BaseApplyFormAction(action_type, action_persistence,
                                         fields, fill_id, supports_refill,
                                         triggered_origin, field_type_map,
                                         section_for_clear_form_on_ios);
        for (const FormFieldData& field : fields) {
          if (filled_fields.contains(field.global_id())) {
            last_filled_values_[field.global_id()] = field.value();
          }
        }
        return filled_fields;
      });
}

FormData ActorTestBase::SeeForm(test::FormDescription form_description) {
  FormData form = test::GetFormData(form_description);
  manager().AddSeenForm(form, test::GetHeuristicTypes(form_description),
                        test::GetServerTypes(form_description));
  return form;
}

TestActorChromeAutofillClient& ActorTestBase::client() {
  return *static_cast<TestActorChromeAutofillClient*>(
      autofill_client_injector_[web_contents()]);
}

TestCreditCardAccessManager& ActorTestBase::credit_card_access_manager() {
  return CHECK_DEREF(static_cast<TestCreditCardAccessManager*>(
      manager().GetCreditCardAccessManager()));
}

PaymentsDataManager& ActorTestBase::payments_data_manager() {
  return client().GetPersonalDataManager().payments_data_manager();
}

TestActorContentAutofillDriver& ActorTestBase::driver() {
  return CHECK_DEREF(autofill_driver_injector_[web_contents()]);
}

TestBrowserAutofillManagerWithTestCCAM& ActorTestBase::manager() {
  return static_cast<TestBrowserAutofillManagerWithTestCCAM&>(
      driver().GetAutofillManager());
}

}  // namespace autofill
