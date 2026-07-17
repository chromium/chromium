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
              ShouldShowPersonalContextAmbientAutofillNotice,
              (),
              (const, override));
  MOCK_METHOD(void,
              MarkPersonalContextAmbientAutofillNoticeAsAcknowledged,
              (),
              (override));
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
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  std::unique_ptr<TouchToFillAutofillDelegateAndroidImpl> delegate_;
};

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       IntendsToShowTouchToFillWhenClientShouldShow) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(true));
  auto [form_id, field_id] = SeeForm();
  EXPECT_TRUE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       DoesNotIntendToShowTouchToFillWhenClientShouldNotShow) {
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(false));
  auto [form_id, field_id] = SeeForm();
  EXPECT_FALSE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       OnNoticeAcknowledgedNotifiesClient) {
  EXPECT_CALL(autofill_client(),
              MarkPersonalContextAmbientAutofillNoticeAsAcknowledged);
  delegate().OnNoticeAcknowledged();
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       TryToShowTouchToFillTriggersClientNotice) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillOnce(Return(true));
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
  EXPECT_TRUE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  EXPECT_TRUE(delegate().IsShowingTouchToFill());
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       TryToShowTouchToFillReturnsFalseWhenClientFails) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice)
      .WillOnce(Return(false));
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
  EXPECT_FALSE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
  EXPECT_FALSE(delegate().IsShowingTouchToFill());
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       TryToShowTouchToFillReturnsFalseWhenClientShouldNotShow) {
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(false));
  EXPECT_CALL(autofill_client(), ShowAmbientAutoFillNotice).Times(0);
  FormData form = test::CreateTestPersonalInformationFormData();
  autofill_manager().AddSeenForm(
      form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
  EXPECT_FALSE(delegate().TryToShowTouchToFill(form, form.fields()[0]));
}

TEST_F(TouchToFillAutofillDelegateAndroidImplTest, HideTouchToFillHidesNotice) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(true));
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

TEST_F(TouchToFillAutofillDelegateAndroidImplTest, OnDismissedResetsState) {
  auto* mock_ai_manager = static_cast<MockAutofillAiManager*>(
      autofill_client().GetAutofillAiManager());
  ON_CALL(*mock_ai_manager, GetSuggestions)
      .WillByDefault(Return(CreatePersonalContextSuggestions()));
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(true));
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

TEST_F(TouchToFillAutofillDelegateAndroidImplTest,
       DoesNotIntendToShowTouchToFillWhenSuggestionsAreMissing) {
  EXPECT_CALL(autofill_client(), ShouldShowPersonalContextAmbientAutofillNotice)
      .WillOnce(Return(true));
  auto [form_id, field_id] = SeeForm();
  EXPECT_FALSE(delegate().IntendsToShowTouchToFill(form_id, field_id));
}

}  // namespace
}  // namespace autofill
