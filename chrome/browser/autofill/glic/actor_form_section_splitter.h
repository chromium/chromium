// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FORM_SECTION_SPLITTER_H_
#define CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FORM_SECTION_SPLITTER_H_

#include "base/containers/span.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillManager;
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
  kContactInfo,
  // Fill only the following physical address portion.
  kAddress
};

// Returns whether a form fill request should be split into two separate
// virtual requests: one for contact information (name, email, phone) and one
// for physical address information.
//
// Splitting occurs if the form contains a phone number or email field that
// comes before any address-related field.
bool ShouldSplitOutContactInfo(
    base::span<const FieldGlobalId> representative_fields,
    const AutofillManager& autofill_manager,
    LogManager* log_manager);

}  // namespace actor

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FORM_SECTION_SPLITTER_H_
