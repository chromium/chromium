// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_form_section_splitter.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::actor {

namespace {

using ::testing::UnorderedElementsAre;

class ActorFormSectionSplitterTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorFormSectionSplitterTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));
  }

  FormData SeeForm(test::FormDescription form_description) {
    FormData form = test::GetFormData(form_description);
    manager().AddSeenForm(form, test::GetHeuristicTypes(form_description),
                          test::GetServerTypes(form_description));
    return form;
  }

 protected:
  TestContentAutofillClient& client() {
    return *autofill_client_injector_[web_contents()];
  }
  TestBrowserAutofillManager& manager() {
    return *autofill_manager_injector_[web_contents()];
  }
  LogManager* log_manager() { return client().GetCurrentLogManager(); }

  base::test::ScopedFeatureList feature_list_;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<TestContentAutofillDriver>
      autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
};

// Tests that ShouldSplitOutContactInfo returns false on a splittable case if
// kAutofillActorFormFillingSplitOutContactInfo is disabled.
TEST_F(ActorFormSectionSplitterTest,
       ShouldSplitOutContactInfo_FeatureDisabled) {
  feature_list_.InitAndDisableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
}

// Tests that ShouldSplitOutContactInfo returns true for a splittable form.
TEST_F(ActorFormSectionSplitterTest,
       ShouldSplitOutContactInfo_SimpleMixedForm) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  EXPECT_TRUE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                        manager(), log_manager()));
}

// Tests that ShouldSplitOutContactInfo returns false if an address field comes
// before a contact info field.
TEST_F(ActorFormSectionSplitterTest, ShouldSplitOutContactInfo_AddressFirst) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS}}});

  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
}

// Tests that ShouldSplitOutContactInfo returns false if there are no contact
// info fields.
TEST_F(ActorFormSectionSplitterTest, ShouldSplitOutContactInfo_NoContactInfo) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
}

// Tests that ShouldSplitOutContactInfo returns false if there are no address
// fields, even if there is a contact info field.
TEST_F(ActorFormSectionSplitterTest, ShouldSplitOutContactInfo_NoAddress) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm(
      {.fields = {{.server_type = NAME_FULL}, {.server_type = EMAIL_ADDRESS}}});

  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
}

// Tests that ShouldSplitOutContactInfo only considers fields that are in the
// same section as the trigger field.
TEST_F(ActorFormSectionSplitterTest,
       ShouldSplitOutContactInfo_DifferentSections) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm(
      {.fields = {{.server_type = EMAIL_ADDRESS,
                   .autocomplete_attribute = "section-s1 email"},
                  {.server_type = NAME_FULL,
                   .autocomplete_attribute = "section-s2 name"},
                  {.server_type = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "section-s2 address-line1"}}});

  // Trigger on the name field in section s2. This should ignore the email
  // address (as its in section s1) and thus we should not split.
  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[1].global_id()},
                                         manager(), log_manager()));
}

}  // namespace

}  // namespace autofill::actor
