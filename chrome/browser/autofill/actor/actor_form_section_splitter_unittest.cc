// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_form_section_splitter.h"

#include "base/test/metrics/histogram_tester.h"
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

using ::testing::IsEmpty;
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

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo",
      ShouldSplitOutContactInfoResult::kShouldNotSplitFeatureDisabled, 1);
}

// Tests that ShouldSplitOutContactInfo returns false and logs correctly if
// there are no trigger fields.
TEST_F(ActorFormSectionSplitterTest,
       ShouldSplitOutContactInfo_NoTriggerFields) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ShouldSplitOutContactInfo(/*trigger_fields=*/{}, manager(),
                                         log_manager()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo",
      ShouldSplitOutContactInfoResult::kShouldNotSplitNoTriggerFields, 1);
}

// Tests that ShouldSplitOutContactInfo returns false and logs correctly if
// the form is not found.
TEST_F(ActorFormSectionSplitterTest, ShouldSplitOutContactInfo_FormNotFound) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  base::HistogramTester histogram_tester;
  // Use a random non-existent field ID.
  EXPECT_FALSE(
      ShouldSplitOutContactInfo({FieldGlobalId()}, manager(), log_manager()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo",
      ShouldSplitOutContactInfoResult::kShouldNotSplitFormNotFound, 1);
}

// Tests that ShouldSplitOutContactInfo returns true for a splittable form.
TEST_F(ActorFormSectionSplitterTest,
       ShouldSplitOutContactInfo_SimpleMixedForm) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                        manager(), log_manager()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo",
      ShouldSplitOutContactInfoResult::kShouldSplit, 1);
}

// Tests that ShouldSplitOutContactInfo returns false if an address field comes
// before a contact info field.
TEST_F(ActorFormSectionSplitterTest, ShouldSplitOutContactInfo_AddressFirst) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS}}});

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo",
      ShouldSplitOutContactInfoResult::kShouldNotSplitAddressBeforeContactInfo,
      1);
}

// Tests that ShouldSplitOutContactInfo returns false if there are no contact
// info fields.
TEST_F(ActorFormSectionSplitterTest, ShouldSplitOutContactInfo_NoContactInfo) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo",
      ShouldSplitOutContactInfoResult::kShouldNotSplitAddressBeforeContactInfo,
      1);
}

// Tests that ShouldSplitOutContactInfo returns false if there are no address
// fields, even if there is a contact info field.
TEST_F(ActorFormSectionSplitterTest, ShouldSplitOutContactInfo_NoAddress) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillActorFormFillingSplitOutContactInfo);

  FormData form = SeeForm(
      {.fields = {{.server_type = NAME_FULL}, {.server_type = EMAIL_ADDRESS}}});

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ShouldSplitOutContactInfo({form.fields()[0].global_id()},
                                         manager(), log_manager()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo",
      ShouldSplitOutContactInfoResult::kShouldNotSplitNoAddressField, 1);
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

// Tests that if a form section is not being split, RetargetTriggerField should
// just return the input trigger field.
TEST_F(ActorFormSectionSplitterTest, RetargetTriggerField_NoSplit) {
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  // Trigger on the NAME_FULL field, which would always be re-targeted if the
  // form section was being split (to either EMAIL_ADDRESS for contact info or
  // ADDRESS_HOME_LINE1 for address).
  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[0].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[0].global_id());

  // Since the form section is not being split, the original trigger field
  // should be returned.
  base::HistogramTester histogram_tester;
  EXPECT_EQ(RetargetTriggerFieldForSplittingIfNeeded(
                form_structure, original_trigger_field,
                SectionSplitPart::kNoSplit, log_manager()),
            original_trigger_field);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      actor::RetargetTriggerFieldResult::kNotAttemptedNoSplit, 1);
}

// Tests that RetargetTriggerField returns the correct re-targeted trigger field
// for the contact info part of a split form section.
TEST_F(ActorFormSectionSplitterTest, RetargetTriggerField_ContactInfo) {
  FormData form = SeeForm(
      {.fields = {{.server_type = EMAIL_ADDRESS,
                   .autocomplete_attribute = "section-s1 email"},
                  {.server_type = NAME_FULL,
                   .autocomplete_attribute = "section-s2 name"},
                  {.server_type = EMAIL_ADDRESS,
                   .autocomplete_attribute = "section-s2 email"},
                  {.server_type = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "section-s2 address-line1"}}});

  // Trigger on the name field in section s2.
  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[1].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[1].global_id());

  // Should retarget to the EMAIL_ADDRESS in section s2 (index 2), ignoring the
  // one in section s1 (index 0).
  base::HistogramTester histogram_tester;
  const AutofillField* retargeted = RetargetTriggerFieldForSplittingIfNeeded(
      form_structure, original_trigger_field, SectionSplitPart::kContactInfo,
      log_manager());
  EXPECT_EQ(retargeted->global_id(), form.fields()[2].global_id());
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      actor::RetargetTriggerFieldResult::kRetargetedToNewField, 1);
}

// Tests that RetargetTriggerField returns the same field when it is already
// a contact info field.
TEST_F(ActorFormSectionSplitterTest,
       RetargetTriggerField_ContactInfo_SameField) {
  FormData form = SeeForm({.fields = {{.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[0].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[0].global_id());

  base::HistogramTester histogram_tester;
  EXPECT_EQ(RetargetTriggerFieldForSplittingIfNeeded(
                form_structure, original_trigger_field,
                SectionSplitPart::kContactInfo, log_manager()),
            original_trigger_field);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      actor::RetargetTriggerFieldResult::kRetargetedToSameField, 1);
}

// Tests that RetargetTriggerField handles failure when contact info field is
// after the address field (should not happen in practice if
// ShouldSplitOutContactInfo is checked first).
TEST_F(ActorFormSectionSplitterTest,
       RetargetTriggerField_ContactInfo_AddressFirst) {
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = EMAIL_ADDRESS}}});

  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[0].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[0].global_id());

  base::HistogramTester histogram_tester;
  EXPECT_EQ(RetargetTriggerFieldForSplittingIfNeeded(
                form_structure, original_trigger_field,
                SectionSplitPart::kContactInfo, log_manager()),
            original_trigger_field);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      actor::RetargetTriggerFieldResult::kErrorContactInfoAddressFirst, 1);
}

// Tests that RetargetTriggerField handles failure when no contact info is
// found (should not happen in practice if ShouldSplitOutContactInfo is checked
// first).
TEST_F(ActorFormSectionSplitterTest,
       RetargetTriggerField_ContactInfo_NotFound) {
  FormData form = SeeForm(
      {.fields = {{.server_type = NAME_FULL}, {.server_type = COMPANY_NAME}}});

  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[0].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[0].global_id());

  base::HistogramTester histogram_tester;
  EXPECT_EQ(RetargetTriggerFieldForSplittingIfNeeded(
                form_structure, original_trigger_field,
                SectionSplitPart::kContactInfo, log_manager()),
            original_trigger_field);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      actor::RetargetTriggerFieldResult::kErrorContactInfoNotFound, 1);
}

// Tests that RetargetTriggerField returns the correct re-targeted trigger field
// for the address part of a split form section.
TEST_F(ActorFormSectionSplitterTest, RetargetTriggerField_Address) {
  FormData form = SeeForm(
      {.fields = {{.server_type = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "section-s1 address-line1"},
                  {.server_type = NAME_FULL,
                   .autocomplete_attribute = "section-s2 name"},
                  {.server_type = EMAIL_ADDRESS,
                   .autocomplete_attribute = "section-s2 email"},
                  {.server_type = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "section-s2 address-line1"}}});

  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[1].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[1].global_id());

  // Should retarget to the ADDRESS_HOME_LINE1 in section s2 (index 3), ignoring
  // the one in section s1 (index 0).
  base::HistogramTester histogram_tester;
  const AutofillField* retargeted = RetargetTriggerFieldForSplittingIfNeeded(
      form_structure, original_trigger_field, SectionSplitPart::kAddress,
      log_manager());
  EXPECT_EQ(retargeted->global_id(), form.fields()[3].global_id());
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      actor::RetargetTriggerFieldResult::kRetargetedToNewField, 1);
}

// Tests that RetargetTriggerField returns the original trigger field for the
// address part of a split form section if the trigger field was already of an
// address type.
TEST_F(ActorFormSectionSplitterTest,
       RetargetTriggerField_Address_AlreadyAddressField) {
  FormData form = SeeForm({.fields = {{.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY}}});

  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[2].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[2].global_id());

  // Since the trigger field is already an address field, it should be returned
  // as-is, even if it's not the first address field in the section.
  base::HistogramTester histogram_tester;
  EXPECT_EQ(RetargetTriggerFieldForSplittingIfNeeded(
                form_structure, original_trigger_field,
                SectionSplitPart::kAddress, log_manager()),
            original_trigger_field);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      RetargetTriggerFieldResult::kRetargetedToSameField, 1);
}

// Tests that RetargetTriggerField handles failure when no address is found
// (should not happen in practice if ShouldSplitOutContactInfo is checked
// first).
TEST_F(ActorFormSectionSplitterTest, RetargetTriggerField_Address_NotFound) {
  FormData form = SeeForm(
      {.fields = {{.server_type = NAME_FULL}, {.server_type = EMAIL_ADDRESS}}});

  const FormStructure* form_structure =
      manager().FindCachedFormById(form.fields()[0].global_id());
  const AutofillField* original_trigger_field =
      form_structure->GetFieldById(form.fields()[0].global_id());

  base::HistogramTester histogram_tester;
  EXPECT_EQ(RetargetTriggerFieldForSplittingIfNeeded(
                form_structure, original_trigger_field,
                SectionSplitPart::kAddress, log_manager()),
            original_trigger_field);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
      actor::RetargetTriggerFieldResult::kErrorAddressNotFound, 1);
}

class ActorFormSectionSplitterGetBlockedFieldsTest
    : public ActorFormSectionSplitterTest,
      public testing::WithParamInterface<mojom::ActionPersistence> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ActorFormSectionSplitterGetBlockedFieldsTest,
                         testing::Values(mojom::ActionPersistence::kFill,
                                         mojom::ActionPersistence::kPreview));

// Tests that GetBlockedFields returns the expected fields for both the contact
// info and address parts for a splittable form.
TEST_P(ActorFormSectionSplitterGetBlockedFieldsTest,
       GetBlockedFields_SimpleSplit) {
  FormData form = SeeForm({.fields = {{.server_type = NAME_FULL},
                                      {.server_type = EMAIL_ADDRESS},
                                      {.server_type = ADDRESS_HOME_LINE1},
                                      {.server_type = ADDRESS_HOME_CITY},
                                      {.server_type = ADDRESS_HOME_COUNTRY}}});

  const FormStructure& form_structure =
      *manager().FindCachedFormById(form.fields()[0].global_id());

  base::HistogramTester histogram_tester;

  // For kContactInfo split, address fields should be blocked.
  base::flat_set<FieldGlobalId> contact_part_blocked_fields =
      GetBlockedFieldsForSplit(form_structure, form.fields()[0].global_id(),
                               SectionSplitPart::kContactInfo, GetParam());
  EXPECT_THAT(contact_part_blocked_fields,
              UnorderedElementsAre(form.fields()[2].global_id(),
                                   form.fields()[3].global_id(),
                                   form.fields()[4].global_id()));
  if (GetParam() == mojom::ActionPersistence::kFill) {
    // There are two contact info fields (that would not have been blocked).
    histogram_tester.ExpectUniqueSample(
        "Autofill.Actor.ContactInfoSplitting.ContactInfoPartFieldCount", 2, 1);
  } else {
    // Nothing should be recorded for preview.
    histogram_tester.ExpectTotalCount(
        "Autofill.Actor.ContactInfoSplitting.ContactInfoPartFieldCount", 0);
  }

  // For kAddress split, contact info fields should be blocked.
  base::flat_set<FieldGlobalId> address_part_blocked_fields =
      GetBlockedFieldsForSplit(form_structure, form.fields()[2].global_id(),
                               SectionSplitPart::kAddress, GetParam());
  EXPECT_THAT(address_part_blocked_fields,
              UnorderedElementsAre(form.fields()[0].global_id(),
                                   form.fields()[1].global_id()));
  if (GetParam() == mojom::ActionPersistence::kFill) {
    // There are three address fields (that would not have been blocked).
    histogram_tester.ExpectUniqueSample(
        "Autofill.Actor.ContactInfoSplitting.AddressPartFieldCount", 3, 1);
  } else {
    // Nothing should be recorded for preview.
    histogram_tester.ExpectTotalCount(
        "Autofill.Actor.ContactInfoSplitting.AddressPartFieldCount", 0);
  }
}

// Tests that GetBlockedFields correctly handles a 'floating' name field that
// binds to an address part that follows it.
TEST_P(ActorFormSectionSplitterGetBlockedFieldsTest,
       GetBlockedFields_FloatingNames) {
  // EMAIL, NAME, ADDRESS. NAME is "floating" and should bind to ADDRESS because
  // it immediately precedes it.
  FormData form = SeeForm({.fields = {{.server_type = EMAIL_ADDRESS},
                                      {.server_type = NAME_FULL},
                                      {.server_type = ADDRESS_HOME_LINE1}}});

  const FormStructure& form_structure =
      *manager().FindCachedFormById(form.fields()[0].global_id());

  base::HistogramTester histogram_tester;

  // For kContactInfo split, NAME and ADDRESS should be blocked.
  base::flat_set<FieldGlobalId> contact_part_blocked_fields =
      GetBlockedFieldsForSplit(form_structure, form.fields()[0].global_id(),
                               SectionSplitPart::kContactInfo, GetParam());
  EXPECT_THAT(contact_part_blocked_fields,
              UnorderedElementsAre(form.fields()[1].global_id(),
                                   form.fields()[2].global_id()));
  if (GetParam() == mojom::ActionPersistence::kFill) {
    // Since the name should bind to the address part, there is only one contact
    // info part field.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Actor.ContactInfoSplitting.ContactInfoPartFieldCount", 1, 1);
  } else {
    // Nothing should be recorded for preview.
    histogram_tester.ExpectTotalCount(
        "Autofill.Actor.ContactInfoSplitting.ContactInfoPartFieldCount", 0);
  }

  // For kAddress split, EMAIL should be blocked.
  base::flat_set<FieldGlobalId> address_part_blocked_fields =
      GetBlockedFieldsForSplit(form_structure, form.fields()[2].global_id(),
                               SectionSplitPart::kAddress, GetParam());
  EXPECT_THAT(address_part_blocked_fields,
              UnorderedElementsAre(form.fields()[0].global_id()));
  if (GetParam() == mojom::ActionPersistence::kFill) {
    // Since the name should bind to the address part, there are two address
    // part fields.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Actor.ContactInfoSplitting.AddressPartFieldCount", 2, 1);
  } else {
    // Nothing should be recorded for preview.
    histogram_tester.ExpectTotalCount(
        "Autofill.Actor.ContactInfoSplitting.AddressPartFieldCount", 0);
  }
}

// Tests that GetBlockedFields correctly handles a trailing 'floating' name
// field due to being passed a non-splittable form section.
//
// This is not intended to be supported, but is required for now as splitting is
// determined based on just the first trigger field for a given FillRequest -
// which may not be an accurate decision for other fields.
TEST_P(ActorFormSectionSplitterGetBlockedFieldsTest,
       GetBlockedFields_TrailingFloatingNames) {
  // EMAIL, NAME. NAME will be "floating" because we never see another field
  // after it, but should bind to the contact_info part.
  FormData form = SeeForm(
      {.fields = {{.server_type = EMAIL_ADDRESS}, {.server_type = NAME_FULL}}});

  const FormStructure& form_structure =
      *manager().FindCachedFormById(form.fields()[0].global_id());

  base::HistogramTester histogram_tester;

  // For kContactInfo split, nothing should be blocked, because the NAME field
  // should have been bound into the contact info part.
  base::flat_set<FieldGlobalId> contact_part_blocked_fields =
      GetBlockedFieldsForSplit(form_structure, form.fields()[0].global_id(),
                               SectionSplitPart::kContactInfo, GetParam());
  EXPECT_THAT(contact_part_blocked_fields, IsEmpty());
  if (GetParam() == mojom::ActionPersistence::kFill) {
    // Both fields are part of the contact info part.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Actor.ContactInfoSplitting.ContactInfoPartFieldCount", 2, 1);
  } else {
    // Nothing should be recorded for preview.
    histogram_tester.ExpectTotalCount(
        "Autofill.Actor.ContactInfoSplitting.ContactInfoPartFieldCount", 0);
  }
}

}  // namespace

}  // namespace autofill::actor
