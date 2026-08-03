// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_delegate_android_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/mock_autofill_ai_manager.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

static constexpr char kTestGuid[] = "00000000-0000-4000-8000-000000000001";

class MockAutofillClient : public TestAutofillClient {
 public:
  using TestAutofillClient::TestAutofillClient;
  MOCK_METHOD(bool,
              ShowAmbientAutoFillNotice,
              (base::WeakPtr<TouchToFillAutofillDelegate> delegate),
              (override));
  MOCK_METHOD(void, HideAmbientAutoFillNotice, (), (override));
};

class TouchToFillAutofillDelegateAndroidImplTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<NiceMock<MockAutofillClient>> {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TouchToFillAutofillDelegateAndroidImplTest() = default;
  ~TouchToFillAutofillDelegateAndroidImplTest() override = default;

  void SetUp() override {
    InitAutofillClient();
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*pcontext_manager=*/nullptr,
            /*strike_database=*/nullptr,
            /*variation_country_code=*/GeoIpCountryCode("US")));
    CreateAutofillDriver();
    delegate_ = std::make_unique<TouchToFillAutofillDelegateAndroidImpl>(
        &autofill_manager());
  }

  void TearDown() override {
    delegate_.reset();
    DestroyAutofillClient();
  }

  TouchToFillAutofillDelegateAndroidImpl& delegate() { return *delegate_; }

  std::vector<Suggestion> CreatePersonalContextSuggestions() {
    std::vector<EntityInstance> entities = {test::GetPassportEntityInstance(
        {.guid = kTestGuid,
         .record_type = EntityInstance::RecordType::kPersonalContext})};
    autofill_client()
        .GetEntityDataManager()
        ->SetPersonalContextEntitiesForTesting(entities);

    Suggestion suggestion(u"label", SuggestionType::kFillAutofillAi);
    suggestion.payload =
        Suggestion::AutofillAiPayload(EntityInstance::EntityId(kTestGuid));
    return {suggestion};
  }

  // Creates a test form, calls `AutofillManager::OnFormsSeen` with it and
  // returns a pair of the form's id and its first field's id.
  std::pair<FormGlobalId, FieldGlobalId> SeeForm() {
    const FormData form = test::CreateTestPersonalInformationFormData();
    autofill_manager().AddSeenForm(
        form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
    return {form.global_id(), form.fields()[0].global_id()};
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  std::unique_ptr<TouchToFillAutofillDelegateAndroidImpl> delegate_;
};

// Verifies that the delegate intends to show TouchToFill when the client allows
// it and personal context suggestions are available.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       IntendsToShowTouchToFillWhenClientShouldShow) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  auto [form_id, field_id] = SeeForm();
  EXPECT_TRUE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

// Verifies that the delegate does not intend to show TouchToFill when the
// client disallows it.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       DoesNotIntendToShowTouchToFillWhenClientShouldNotShow) {
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(false);
  auto [form_id, field_id] = SeeForm();
  EXPECT_FALSE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

// Verifies that acknowledging the notice notifies the client to mark it as
// acknowledged.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       OnNoticeAcknowledgedNotifiesClient) {
  delegate().OnNoticeAcknowledged();
  EXPECT_TRUE(autofill_client()
                  .GetPersonalContextFirstRunService()
                  ->is_ambient_autofill_notice_acknowledged());
}

// Verifies that trying to show TouchToFill successfully triggers the notice on
// the client and updates the internal state.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       TryToShowTouchToFillTriggersClientNotice) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillOnce(Return(true));
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
  EXPECT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  EXPECT_TRUE(delegate().IsShowingTouchToFill());
}

// Verifies that trying to show TouchToFill returns false if the client fails to
// show the notice.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       TryToShowTouchToFillReturnsFalseWhenClientFails) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillOnce(Return(false));
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
  EXPECT_FALSE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
}

// Verifies that trying to show TouchToFill returns false if the client
// disallows showing it.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       TryToShowTouchToFillReturnsFalseWhenClientShouldNotShow) {
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(false);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice).Times(0);
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  EXPECT_FALSE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
}

// Verifies that HideTouchToFill hides the notice via the client and updates the
// state.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest, HideTouchToFillHidesNotice) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillOnce(Return(true));
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  ASSERT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  ASSERT_TRUE(delegate().IsShowingTouchToFill());

  EXPECT_CALL(autofill_client(), HideAmbientAutoFillNotice);
  delegate().HideTouchToFill();
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
}

// Verifies that OnDismissed resets the showing state to inactive.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest, OnDismissedResetsState) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillOnce(Return(true));
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  ASSERT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  ASSERT_TRUE(delegate().IsShowingTouchToFill());

  delegate().OnDismissed();
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
}

// Verifies that OnDismissed triggers standard suggestions and temporarily
// suppresses TouchToFill for the next immediate attempt.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       OnDismissedFiresTaskAndSuppressesTtf) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillRepeatedly(Return(true));

  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));

  ASSERT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  ASSERT_TRUE(delegate().IsShowingTouchToFill());

  delegate().OnDismissed();
  EXPECT_FALSE(delegate().IsShowingTouchToFill());

  // Trying to show again immediately should return false due to suppression.
  EXPECT_FALSE(delegate().TryToShowTouchToFill(form, form.fields()[0]));

  // Subsequent queries should succeed because suppression was consumed.
  EXPECT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
}

// Verifies that acknowledging the notice triggers standard suggestions and
// temporarily suppresses TouchToFill after the sheet is dismissed.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       AcknowledgeFiresTaskAndSuppressesTtf) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillRepeatedly(Return(true));

  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));

  ASSERT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  ASSERT_TRUE(delegate().IsShowingTouchToFill());

  // Acknowledge notice.
  delegate().OnNoticeAcknowledged();
  EXPECT_TRUE(autofill_client()
                  .GetPersonalContextFirstRunService()
                  ->is_ambient_autofill_notice_acknowledged());

  // State should still be showing (waiting for dismissal callback).
  EXPECT_TRUE(delegate().IsShowingTouchToFill());

  // Simulate sheet closed callback.
  delegate().OnDismissed();
  EXPECT_FALSE(delegate().IsShowingTouchToFill());

  // Trying to show again immediately should return false due to suppression.
  EXPECT_FALSE(delegate().TryToShowTouchToFill(form, form.fields()[0]));

  // Subsequent queries should succeed because suppression was consumed.
  EXPECT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
}

// Verifies that OnDismissed does not suppress TouchToFill for different fields
// than the one that was dismissed.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       OnDismissedDoesNotSuppressOtherFields) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillRepeatedly(Return(true));

  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));

  // Show for field 0.
  ASSERT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  ASSERT_TRUE(delegate().IsShowingTouchToFill());

  // Dismiss. State transitions to kSuppressing (for field 0).
  delegate().OnDismissed();
  EXPECT_FALSE(delegate().IsShowingTouchToFill());

  // Try to show for field 1 (different field).
  // It should NOT be suppressed, so TryToShow should return true.
  EXPECT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[1]));
  EXPECT_TRUE(delegate().IsShowingTouchToFill());
}

// Verifies that acknowledging the notice notifies the client, even if forms
// have been seen.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       OnNoticeLinkClickedOrAcknowledgedOnDismissedMatches) {
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));

  // Verify that onsettingslink or notice acknowledge triggers OnDismissed
  delegate().OnNoticeAcknowledged();
  EXPECT_TRUE(autofill_client()
                  .GetPersonalContextFirstRunService()
                  ->is_ambient_autofill_notice_acknowledged());
}

// Verifies that clicking the settings link transitions the state to navigating
// away, which bypasses temporary suppression and suggestion triggering on
// dismissal.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       OnSettingsLinkClickedSetsStateInactiveDirectly) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillRepeatedly(Return(true));

  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));

  ASSERT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  ASSERT_TRUE(delegate().IsShowingTouchToFill());

  delegate().OnSettingsLinkClicked();
  EXPECT_FALSE(delegate().IsShowingTouchToFill());

  delegate().OnDismissed();  // Simulate sheet closing

  // Trying to show again immediately should succeed since suppression was not
  // activated.
  EXPECT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
}

// Verifies that temporary suppression works correctly even if suggestions are
// returned asynchronously after dismissal.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       AsynchronousSuggestionsSuppressCorrectly) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());

  // Setup mock to return suggestions initially so the sheet can be shown.
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillRepeatedly(Return(true));

  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));

  // Show the ttf first.
  ASSERT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  ASSERT_TRUE(delegate().IsShowingTouchToFill());

  // Setup mock to return NO suggestions during the sync
  // TriggerAskForValuesToFill query, simulating asynchronous DB latency.
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(std::vector<Suggestion>{}));

  // Dismiss the ttf.
  delegate().OnDismissed();
  EXPECT_FALSE(delegate().IsShowingTouchToFill());

  // Return personal context suggestions (simulates DB callback completing).
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));

  // Manually call TryToShowTouchToFill (simulating asynchronous callback).
  // It should see suppress_touch_to_fill_ is still true, consume it, and
  // return false.
  EXPECT_FALSE(delegate().TryToShowTouchToFill(form, form.fields()[0]));

  // Subsequent queries should succeed because suppression was consumed.
  EXPECT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
}

// Verifies that the delegate does not intend to show TouchToFill if there are
// no personal context suggestions available.
TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       DoesNotIntendToShowTouchToFillWhenSuggestionsAreMissing) {
  autofill_client()
      .GetPersonalContextFirstRunService()
      ->set_should_show_ambient_autofill_notice(true);
  auto [form_id, field_id] = SeeForm();
  EXPECT_FALSE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

}  // namespace
}  // namespace autofill
