// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_form_section_splitter.h"

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

}  // namespace

bool ShouldSplitOutContactInfo(
    base::span<const FieldGlobalId> representative_fields,
    const AutofillManager& autofill_manager,
    LogManager* log_manager) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillActorFormFillingSplitOutContactInfo)) {
    return false;
  }

  if (representative_fields.empty()) {
    return false;
  }

  // For now, we simply take the first field.
  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(representative_fields[0]);
  if (!form_structure) {
    LOG_AF(log_manager)
        << LoggingScope::kAutofillActor
        << "Could not find form structure for first trigger field.";
    return false;
  }

  const AutofillField* trigger_field =
      form_structure->GetFieldById(representative_fields[0]);

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

}  // namespace autofill::actor
