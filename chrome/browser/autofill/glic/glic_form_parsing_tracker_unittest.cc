// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/glic_form_parsing_tracker.h"

#include "base/test/task_environment.h"
#include "chrome/browser/autofill/glic/glic_form_parsing_tracker_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class GlicFormParsingTrackerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 public:
  GlicFormParsingTrackerTest() {
    InitAutofillClient();
    tracker_ = std::make_unique<GlicFormParsingTracker>(&autofill_client());
    CreateAutofillDriver();
  }

  ~GlicFormParsingTrackerTest() override { DestroyAutofillClient(); }

  GlicFormParsingTracker& tracker() { return *tracker_; }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_unit_test_environment_;
  std::unique_ptr<GlicFormParsingTracker> tracker_;
};

// Tests that when a new form is discovered (`OnBeforeFormsSeen`), it is added
// to the internal tracking map with both parsing bits initialized to false.
TEST_F(GlicFormParsingTrackerTest, FormAddedOnSeen) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  const absl::flat_hash_map<
      FormGlobalId, GlicFormParsingTracker::FormParsingStatus>& status_map =
      test_api(tracker()).form_parsing_status();
  ASSERT_TRUE(status_map.contains(form_id));
  EXPECT_FALSE(status_map.at(form_id).heuristic_parsed_in_actor_mode);
  EXPECT_FALSE(status_map.at(form_id).server_parsed_in_actor_mode);
}

// Tests that when a form is removed from the DOM (`OnBeforeFormsSeen` with
// removed_forms), it is successfully purged from the internal tracking map.
TEST_F(GlicFormParsingTrackerTest, FormRemoved) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};

  // Add the form first.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());
  ASSERT_EQ(test_api(tracker()).form_parsing_status().size(), 1u);

  // Notify that the form was removed.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, base::span<FormGlobalId>(),
      forms);
  EXPECT_TRUE(test_api(tracker()).form_parsing_status().empty());
}

// Tests that when the AutofillManager's lifecycle state changes from active to
// inactive, all forms associated with that manager's frame are removed from
// tracking, while forms in other frames are preserved.
TEST_F(GlicFormParsingTrackerTest, CleanupOnLifecycleChange) {
  LocalFrameToken frame_token = autofill_driver().GetFrameToken();
  FormGlobalId form_in_frame = {frame_token, FormRendererId(123)};

  LocalFrameToken other_frame_token(base::UnguessableToken::Create());
  FormGlobalId form_in_other_frame = {other_frame_token, FormRendererId(456)};

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_in_frame, form_in_other_frame},
      base::span<FormGlobalId>());

  ASSERT_EQ(test_api(tracker()).form_parsing_status().size(), 2u);

  // Simulate the manager's frame becoming inactive.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAutofillManagerStateChanged,
      /*previous=*/AutofillDriver::LifecycleState::kActive,
      /*current=*/AutofillDriver::LifecycleState::kInactive);

  // Verify only the form associated with the inactive frame was removed.
  const auto& status_map = test_api(tracker()).form_parsing_status();
  EXPECT_FALSE(status_map.contains(form_in_frame))
      << "Form in the deactivated frame should have been erased.";
  EXPECT_TRUE(status_map.contains(form_in_other_frame))
      << "Form in a different frame should still be tracked.";
}

// Tests that `OnAutofillManagerStateChanged`'s cleanup logic is NOT triggered
// when the state change does not involve transitioning away from kActive (e.g.,
// kInactive to kActive).
TEST_F(GlicFormParsingTrackerTest, NoCleanupOnActivation) {
  LocalFrameToken frame_token = autofill_driver().GetFrameToken();
  FormGlobalId form_id = {frame_token, FormRendererId(123)};

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  // Transition from kInactive to kActive should NOT trigger erasure.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAutofillManagerStateChanged,
      /*previous=*/AutofillDriver::LifecycleState::kInactive,
      /*current=*/AutofillDriver::LifecycleState::kActive);

  EXPECT_TRUE(test_api(tracker()).form_parsing_status().contains(form_id));
}

// Tests the state transitions of a form. It verifies that heuristic and
// server parsing statuses are updated independently based on the source
// provided in `OnFieldTypesDetermined`.
TEST_F(GlicFormParsingTrackerTest, StateTransitions) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  // Simulate local heuristic parsing completion.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      /*small_forms_were_parsed=*/true);
  EXPECT_TRUE(test_api(tracker())
                  .form_parsing_status()
                  .at(form_id)
                  .heuristic_parsed_in_actor_mode);
  EXPECT_FALSE(test_api(tracker())
                   .form_parsing_status()
                   .at(form_id)
                   .server_parsed_in_actor_mode);

  // Simulate server-side parsing completion.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer,
      /*small_forms_were_parsed=*/true);
  EXPECT_TRUE(test_api(tracker())
                  .form_parsing_status()
                  .at(form_id)
                  .server_parsed_in_actor_mode);
}

// Tests that if a form is seen again (e.g., the DOM was modified and
// re-triggered `OnBeforeFormsSeen`), its parsing status is reset to false
// because the previous parsing results may no longer be valid.
TEST_F(GlicFormParsingTrackerTest, ResetOnReSeen) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      /*small_forms_were_parsed=*/true);

  // Simulate the same form being updated/re-processed by the manager.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  // The status should be cleared (reset to default).
  EXPECT_FALSE(test_api(tracker())
                   .form_parsing_status()
                   .at(form_id)
                   .heuristic_parsed_in_actor_mode);
}

// Tests that the detector respects the `small_forms_were_parsed` flag. If the
// manager determines types but small forms were NOT parsed, the detector
// should not mark the form as parsed.
TEST_F(GlicFormParsingTrackerTest, IgnoreSmallForms) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      /*small_forms_were_parsed=*/false);

  EXPECT_FALSE(test_api(tracker())
                   .form_parsing_status()
                   .at(form_id)
                   .heuristic_parsed_in_actor_mode);
}

}  // namespace

}  // namespace autofill
