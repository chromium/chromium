// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_form_section_splitter.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
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
  auto record_outcome_metric = [](RetargetTriggerFieldResult result) {
    base::UmaHistogramEnumeration(
        "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField", result);
  };

  // For contact info we retarget to the first email or phone number field in
  // the section, which should come before any address field assuming that
  // ShouldSplitOutContactInfo was called and returned true.
  for (const std::unique_ptr<AutofillField>& field : fields) {
    if (field->section() != original_trigger_field->section()) {
      continue;
    }

    if (HasContactInfoType(*field)) {
      record_outcome_metric(
          field.get() == original_trigger_field
              ? RetargetTriggerFieldResult::kRetargetedToSameField
              : RetargetTriggerFieldResult::kRetargetedToNewField);
      return field.get();
    }

    // This shouldn't happen as long as ShouldSplitOutContactInfo was called
    // first, but handle it in case.
    if (field->Type().GetGroups().contains(FieldTypeGroup::kAddress)) {
      LOG_AF(log_manager)
          << LoggingScope::kAutofillActor
          << "GetTriggerFieldForContactInfoPart found an address field before "
             "a contact info field for a kContactInfo split.";
      record_outcome_metric(
          RetargetTriggerFieldResult::kErrorContactInfoAddressFirst);
      return original_trigger_field;
    }
  }

  // This shouldn't happen as long as ShouldSplitOutContactInfo was called
  // first, but handle it in case.
  LOG_AF(log_manager) << LoggingScope::kAutofillActor
                      << "GetTriggerFieldForContactInfoPart could not find an "
                         "appropriate kContactInfo trigger field.";
  record_outcome_metric(RetargetTriggerFieldResult::kErrorContactInfoNotFound);
  return original_trigger_field;
}

// Returns an appropriate trigger field for the address split part of a form
// section.
const AutofillField* GetTriggerFieldForAddressPart(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    const AutofillField* original_trigger_field,
    LogManager* log_manager) {
  auto record_outcome_metric = [](RetargetTriggerFieldResult result) {
    base::UmaHistogramEnumeration(
        "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField", result);
  };

  // If the original trigger is already an address field, we can just return
  // that field, as it is guaranteed to be in the address part of the section.
  if (original_trigger_field->Type().GetGroups().contains(
          FieldTypeGroup::kAddress)) {
    record_outcome_metric(RetargetTriggerFieldResult::kRetargetedToSameField);
    return original_trigger_field;
  }

  // Otherwise, re-target to the first address field in the form section.
  for (const std::unique_ptr<AutofillField>& field : fields) {
    if (field->section() != original_trigger_field->section()) {
      continue;
    }

    if (field->Type().GetGroups().contains(FieldTypeGroup::kAddress)) {
      record_outcome_metric(
          field.get() == original_trigger_field
              ? RetargetTriggerFieldResult::kRetargetedToSameField
              : RetargetTriggerFieldResult::kRetargetedToNewField);
      return field.get();
    }
  }

  // This shouldn't happen as long as ShouldSplitOutContactInfo was called
  // first, but handle it in case.
  LOG_AF(log_manager) << LoggingScope::kAutofillActor
                      << "GetTriggerFieldForAddressPart could not find an "
                         "appropriate kAddress trigger field.";
  record_outcome_metric(RetargetTriggerFieldResult::kErrorAddressNotFound);
  return original_trigger_field;
}

struct SplitFormSectionFieldsResult {
  base::flat_set<FieldGlobalId> contact_info_fields;
  base::flat_set<FieldGlobalId> address_fields;
};

// Computes the set of split form fields (represented via their FieldGlobalIds)
// for a given form section represented by `trigger_field`.
SplitFormSectionFieldsResult SplitFormSectionFields(
    const FormStructure& form_structure,
    const AutofillField& trigger_field) {
  SplitFormSectionFieldsResult result;
  std::vector<FieldGlobalId> floating_names;
  // Tracks whether the field processing is currently in the preceding "contact
  // info" part or the subsequent "address" part of the form section. See the
  // documentation on actor::SectionSplitPart for more details.
  bool in_contact_info = true;

  auto commit_floating_names = [&]() {
    if (in_contact_info) {
      result.contact_info_fields.insert_range(floating_names);
    } else {
      result.address_fields.insert_range(floating_names);
    }
    floating_names.clear();
  };

  // Process a field from the form, internally tracking it as either part of the
  // contact info split or the address split. Each new input field is assumed to
  // come after all previous fields passed to ProcessField.
  auto process_field = [&](const AutofillField& field) {
    const FieldGlobalId field_id = field.global_id();

    if (field.Type().GetGroups().contains(FieldTypeGroup::kName)) {
      // If we are still in the contact info part and hit a name field, it may
      // either be in the contact info part, or it may belong to the subsequent
      // address part (if followed directly by an address field).
      //
      // To handle this, name fields "float" until we decide whether they should
      // definitely be in the contact info part or should be in an immediately
      // following address part. See documentation on actor::SectionSplitPart
      // for details.
      if (in_contact_info) {
        floating_names.push_back(field_id);
        return;
      }
    }

    // If we have hit an email or phone number and still have any floating
    // names, they should bind to contact info.
    if (in_contact_info && HasContactInfoType(field) &&
        !floating_names.empty()) {
      commit_floating_names();
    }

    if (field.Type().GetGroups().contains(FieldTypeGroup::kAddress)) {
      // Switch to address part.
      in_contact_info = false;

      // Add any still-floating names to the address part.
      if (!floating_names.empty()) {
        commit_floating_names();
      }
    }

    if (in_contact_info) {
      result.contact_info_fields.insert(field_id);
    } else {
      result.address_fields.insert(field_id);
    }
  };

  for (const std::unique_ptr<AutofillField>& field : form_structure.fields()) {
    if (field->section() == trigger_field.section()) {
      process_field(*field);
    }
  }

  // Commit any remaining floating names. This shouldn't be necessary if
  // ShouldSplitOutContactInfo returned true for `trigger_field`, however at
  // the current time splitting (like suggestion generation) is decided based
  // only on the first trigger field in possibly multiple trigger fields for a
  // given FillRequest.
  //
  // TODO(crbug.com/455788947): Once we determine how to handle multiple
  // trigger fields, remove this and replace it with a CHECK that
  // `floating_names_` is empty, and also DCHECK that ShouldSplitContactInfo
  // returns true for the `trigger_field`.
  commit_floating_names();

  return result;
}

}  // namespace

bool ShouldSplitOutContactInfo(
    base::span<const FieldGlobalId> trigger_fields,
    const AutofillManager& autofill_manager,
    LogManager* log_manager) {
  // TODO(crbug.com/491031514): Consider moving metric record to
  // ActorFormFillingServiceImpl, in order to ensure it is only recorded once
  // per FormFillingRequest.
  auto record_outcome_metric = [](ShouldSplitOutContactInfoResult status) {
    base::UmaHistogramEnumeration(
        "Autofill.Actor.ContactInfoSplitting.ShouldSplitContactInfo", status);
  };

  if (!base::FeatureList::IsEnabled(
          features::kAutofillActorFormFillingSplitOutContactInfo)) {
    record_outcome_metric(
        ShouldSplitOutContactInfoResult::kShouldNotSplitFeatureDisabled);
    return false;
  }

  if (trigger_fields.empty()) {
    record_outcome_metric(
        ShouldSplitOutContactInfoResult::kShouldNotSplitNoTriggerFields);
    return false;
  }

  // For now, we simply take the first field.
  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(trigger_fields[0]);
  if (!form_structure) {
    LOG_AF(log_manager)
        << LoggingScope::kAutofillActor
        << "Could not find form structure for first trigger field.";
    record_outcome_metric(
        ShouldSplitOutContactInfoResult::kShouldNotSplitFormNotFound);
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
      record_outcome_metric(seen_contact_info
                                ? ShouldSplitOutContactInfoResult::kShouldSplit
                                : ShouldSplitOutContactInfoResult::
                                      kShouldNotSplitAddressBeforeContactInfo);
      return seen_contact_info;
    }
  }

  // We never saw an address field, so we shouldn't split.
  record_outcome_metric(
      ShouldSplitOutContactInfoResult::kShouldNotSplitNoAddressField);
  return false;
}

const AutofillField* RetargetTriggerFieldForSplittingIfNeeded(
    const FormStructure* form_structure,
    const AutofillField* original_trigger_field,
    SectionSplitPart split_part,
    LogManager* log_manager) {
  // TODO(crbug.com/491031514): Consider moving metric record to
  // ActorFormFillingServiceImpl, in order to ensure it is only recorded once
  // per each split of a FormFillingRequest.
  switch (split_part) {
    case SectionSplitPart::kNoSplit:
      base::UmaHistogramEnumeration(
          "Autofill.Actor.ContactInfoSplitting.RetargetTriggerField",
          RetargetTriggerFieldResult::kNotAttemptedNoSplit);
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

base::flat_set<FieldGlobalId> GetBlockedFieldsForSplit(
    const FormStructure& form_structure,
    const FieldGlobalId& trigger_field_id,
    SectionSplitPart split_part,
    mojom::ActionPersistence action_persistence) {
  if (split_part == SectionSplitPart::kNoSplit) {
    return {};
  }

  const AutofillField* trigger_field =
      form_structure.GetFieldById(trigger_field_id);
  if (!trigger_field) {
    return {};
  }

  SplitFormSectionFieldsResult split_fields =
      SplitFormSectionFields(form_structure, *trigger_field);

  auto record_outcome_metric = [&](std::string_view suffix,
                                   size_t field_count) {
    // Metrics are only recorded when we are filling, to avoid over-reporting
    // due to previews.
    //
    // TODO(crbug.com/491031514): Consider moving metric record to
    // ActorFormFillingServiceImpl, to avoid this function having to know about
    // the fill mode.
    if (action_persistence == mojom::ActionPersistence::kFill) {
      base::UmaHistogramCounts100(
          base::StrCat({"Autofill.Actor.ContactInfoSplitting.", suffix}),
          field_count);
    }
  };

  // Because this function returns a blocklist of fields, we return the
  // inverse set of fields for the given split part.
  switch (split_part) {
    case SectionSplitPart::kContactInfo:
      record_outcome_metric("ContactInfoPartFieldCount",
                            split_fields.contact_info_fields.size());
      // When filling contact info, block address fields.
      return std::move(split_fields.address_fields);
    case SectionSplitPart::kAddress:
      record_outcome_metric("AddressPartFieldCount",
                            split_fields.address_fields.size());
      // When filling address info, block contact info fields.
      return std::move(split_fields.contact_info_fields);
    case SectionSplitPart::kNoSplit:
      // Handled above, to avoid parsing the form in the no-split case.
      NOTREACHED();
  }
}

}  // namespace autofill::actor
