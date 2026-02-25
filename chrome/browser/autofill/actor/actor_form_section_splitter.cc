// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_form_section_splitter.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"

namespace autofill::actor {
namespace {

// Returns whether or not the input `field` has a Type() that contains an email
// or phone FieldTypeGroup.
bool HasContactInfoType(const AutofillField& field) {
  return field.Type().GetGroups().contains_any(
      {FieldTypeGroup::kEmail, FieldTypeGroup::kPhone});
}

// Returns an appropriate trigger field for the contact info split part of a
// form section.
const AutofillField* GetTriggerFieldForContactInfoPart(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const AutofillField* original_trigger_field,
    LogManager* log_manager) {
  // For contact info we retarget to the first email or phone number field in
  // the section, which should come before any address field assuming that
  // ShouldSplitOutContactInfo was called and returned true.
  for (const std::unique_ptr<AutofillField>& field : fields) {
    if (field->section() != original_trigger_field->section()) {
      continue;
    }

    if (HasContactInfoType(*field)) {
      return field.get();
    }

    // This shouldn't happen as long as ShouldSplitOutContactInfo was called
    // first, but handle it in case.
    if (field->Type().GetGroups().contains(FieldTypeGroup::kAddress)) {
      LOG_AF(log_manager)
          << LoggingScope::kAutofillActor
          << "GetTriggerFieldForContactInfoPart found an address field before "
             "a contact info field for a kContactInfo split.";
      return original_trigger_field;
    }
  }

  // This shouldn't happen as long as ShouldSplitOutContactInfo was called
  // first, but handle it in case.
  LOG_AF(log_manager) << LoggingScope::kAutofillActor
                      << "GetTriggerFieldForContactInfoPart could not find an "
                         "appropriate kContactInfo trigger field.";
  return original_trigger_field;
}

// Returns an appropriate trigger field for the address split part of a form
// section.
const AutofillField* GetTriggerFieldForAddressPart(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const AutofillField* original_trigger_field,
    LogManager* log_manager) {
  // If the original trigger is already an address field, we can just return
  // that field, as it is guaranteed to be in the address part of the section.
  if (original_trigger_field->Type().GetGroups().contains(
          FieldTypeGroup::kAddress)) {
    return original_trigger_field;
  }

  // Otherwise, re-target to the first address field in the form section.
  for (const std::unique_ptr<AutofillField>& field : fields) {
    if (field->section() != original_trigger_field->section()) {
      continue;
    }

    if (field->Type().GetGroups().contains(FieldTypeGroup::kAddress)) {
      return field.get();
    }
  }

  // This shouldn't happen as long as ShouldSplitOutContactInfo was called
  // first, but handle it in case.
  LOG_AF(log_manager) << LoggingScope::kAutofillActor
                      << "GetTriggerFieldForAddressPart could not find an "
                         "appropriate kAddress trigger field.";
  return original_trigger_field;
}

}  // namespace

bool ShouldSplitOutContactInfo(
    base::span<const FieldGlobalId> trigger_fields,
    const AutofillManager& autofill_manager,
    LogManager* log_manager) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillActorFormFillingSplitOutContactInfo)) {
    return false;
  }

  if (trigger_fields.empty()) {
    return false;
  }

  // For now, we simply take the first field.
  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(trigger_fields[0]);
  if (!form_structure) {
    LOG_AF(log_manager)
        << LoggingScope::kAutofillActor
        << "Could not find form structure for first trigger field.";
    return false;
  }

  const AutofillField* trigger_field =
      form_structure->GetFieldById(trigger_fields[0]);

  // Splitting occurs if the form section contains a phone number or email field
  // that comes before any address-related field.
  bool seen_contact_info = false;
  for (const std::unique_ptr<AutofillField>& field : form_structure->fields()) {
    if (field->section() != trigger_field->section()) {
      continue;
    }

    if (HasContactInfoType(*field)) {
      seen_contact_info = true;
      continue;
    }

    if (field->Type().GetGroups().contains(FieldTypeGroup::kAddress)) {
      // We have either seen a contact info field already, and so should split,
      // or the address field is first and so we should not split.
      return seen_contact_info;
    }
  }

  // We never saw an address field, so we shouldn't split.
  return false;
}

const AutofillField* RetargetTriggerFieldForSplittingIfNeeded(
    const FormStructure* form_structure,
    const AutofillField* original_trigger_field,
    SectionSplitPart split_part,
    LogManager* log_manager) {
  switch (split_part) {
    case SectionSplitPart::kNoSplit:
      return original_trigger_field;
    case SectionSplitPart::kContactInfo:
      return GetTriggerFieldForContactInfoPart(
          form_structure->fields(), original_trigger_field, log_manager);
    case SectionSplitPart::kAddress:
      return GetTriggerFieldForAddressPart(form_structure->fields(),
                                           original_trigger_field, log_manager);
  }

  NOTREACHED();
}

}  // namespace autofill::actor
