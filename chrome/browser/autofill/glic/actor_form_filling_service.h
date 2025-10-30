// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FORM_FILLING_SERVICE_H_
#define CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FORM_FILLING_SERVICE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace tabs {
class TabInterface;
}

namespace autofill {

// Interface for `actor::ExecutionEngine` to communicate with Autofill
// functionality.
class ActorFormFillingService {
 public:
  virtual ~ActorFormFillingService() = default;

  // Creates a filling proposal for each of the "actor form"s defined in
  // `fill_requests` and calls callback with it.
  // Here an "actor form" is the union of one or more sections of a `FormData`
  // and the section is identified by the `FieldGlobalId` of an arbitrary
  // field inside that section.
  using FillRequest = std::pair<ActorFormFillingRequest::RequestedData,
                                std::vector<FieldGlobalId>>;
  virtual void GetSuggestions(
      const tabs::TabInterface& tab,
      base::span<const FillRequest> fill_requests,
      base::OnceCallback<
          void(base::expected<std::vector<ActorFormFillingRequest>,
                              ActorFormFillingError>)> callback) = 0;

  // Attempts to fill `chosen_suggestions` and notifies `callback` with the
  // result.
  virtual void FillSuggestions(
      const tabs::TabInterface& tab,
      base::span<const ActorFormFillingSelection> chosen_suggestions,
      base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>
          callback) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FORM_FILLING_SERVICE_H_
