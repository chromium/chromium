// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_SECTION_SPLITTER_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_SECTION_SPLITTER_H_

#include "base/containers/span.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;
class AutofillManager;
class FormStructure;
class LogManager;

namespace actor {

// Represents which part of a split form section is being filled.
//
// Splitting is performed when we believe a single form section contains two
// semantically distinct parts - an initial "contact info" part and a
// following "physical address" part - and we want to fill them separately.
enum class SectionSplitPart {
  // No splitting; fill the entire section.
  kNoSplit,

  // Fill only the preceding contact information portion.
  //
  // The contact information part of a split form section is defined as all
  // fields in the section up to the first field that has an address type.
  // Example:
  //
  //     <!-- Contact information part starts. -->
  //     <input autocomplete="name" />
  //     <input autocomplete="email" />
  //     <!-- Contact information part ends. -->
  //     <input autocomplete="street-address" />
  //     <input autocomplete="tel" />
  //
  // The exception are trailing name type fields. If one or more name fields
  // come right before the first address field, they bind to the address portion
  // instead. Example:
  //     <!-- Contact information part starts. -->
  //     <input autocomplete="email" />
  //     <!-- Contact information part ends. The name field 'binds' to the
  //          address part that follows, see below. -->
  //     <input autocomplete="name" />
  //     <input autocomplete="street-address" />
  //     <input autocomplete="tel" />
  kContactInfo,

  // Fill only the following physical address portion.
  //
  // The physical address part of a split form section is defined as all fields
  // in the section from the first field with an address type, or the first
  // occurrence of a name field that only has name or address fields immediately
  // following it.
  // Example:
  //     <input autocomplete="email" />
  //     <!-- Address part starts; it includes the name fields here as they
  //          directly precede an address type field. -->
  //     <input autocomplete="given-name" />
  //     <input autocomplete="family-name" />
  //     <input autocomplete="street-address" />
  //     <input autocomplete="tel" />
  //     <!-- Address part ends. -->
  kAddress
};

// Returns whether a form fill request should be split into two separate virtual
// requests: one for contact information and one for address information.
//
// Splitting occurs if the form contains a phone number or email field that
// comes before any address-related field. The occurrence and position of name
// fields does not matter here.
bool ShouldSplitOutContactInfo(
    base::span<const FieldGlobalId> trigger_fields,
    const AutofillManager& autofill_manager,
    LogManager* log_manager);

// Given an `original_trigger_field` and `form_structure` identifying a fill
// into a particular form section and an associated `split_part` indicating
// whether or not the fill is being split, retarget the trigger field if needed
// to match the specific `split_part`.
const AutofillField* RetargetTriggerFieldForSplittingIfNeeded(
    const FormStructure* form_structure,
    const AutofillField* original_trigger_field,
    SectionSplitPart split_part,
    LogManager* log_manager);

}  // namespace actor

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_SECTION_SPLITTER_H_
