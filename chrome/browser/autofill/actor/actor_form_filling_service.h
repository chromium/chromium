// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_FILLING_SERVICE_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_FILLING_SERVICE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace tabs {
class TabInterface;
}

namespace autofill {

// Interface for the actor tooling to communicate with Autofill functionality
// for form filling.
//
// Each instance of this interface is associated with a single high level
// request from the actor to fill forms, though that request may be for multiple
// fills across multiple types of data and form sections.
class ActorFormFillingService {
 public:
  virtual ~ActorFormFillingService() = default;

  // Represents a request from the actor to fill one or more form sections.
  //
  // The RequestedData identifies the 'type' of data to fill (e.g., shipping
  // address, billing address, or credit card). Each FieldGlobalId in the vector
  // identifies a 'trigger' field in a form section that the actor wants to
  // fill. Multiple trigger fields are supported to allow the actor to indicate
  // that these form sections should be filled with the same data (i.e., that
  // they are part of the same overall form).
  using FillRequest = std::pair<ActorFormFillingRequest::RequestedData,
                                std::vector<FieldGlobalId>>;

  // Retrieves Autofill suggestions for a set of fill requests from the actor.
  //
  // For each FillRequest, Autofill data will be retrieved based on the
  // RequestedData type and the trigger fields identified by the
  // FieldGlobalIds. The suggestions are returned via the callback, in the same
  // order as the FillRequests. If an error occurs, the callback will be invoked
  // with an ActorFormFillingError.
  //
  // The returned suggestions are expected to be shown to the user in a UX,
  // from which the user will make selections (one per fill request). The
  // selected suggestions should subsequently be passed to FillSuggestions().
  virtual void GetSuggestions(
      const tabs::TabInterface& tab,
      base::span<const FillRequest> fill_requests,
      base::OnceCallback<
          void(base::expected<std::vector<ActorFormFillingRequest>,
                              ActorFormFillingError>)> callback) = 0;

  // Attempts to fill the `chosen_suggestions` into their corresponding form
  // sections. The suggestions must have been obtained from a prior call to
  // GetSuggestions().
  //
  // If successful, the callback will be invoked with a void value. If an error
  // occurs, the callback will be invoked with an ActorFormFillingError.
  virtual void FillSuggestions(
      const tabs::TabInterface& tab,
      base::span<const ActorFormFillingSelection> chosen_suggestions,
      base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>
          callback) = 0;

  // Scrolls the form into view.
  // `form_index` corresponds to the vector of ActorFormFillingRequests
  // retrieved by GetSuggestions().
  virtual void ScrollToForm(const tabs::TabInterface& tab, int form_index) = 0;

  // Previews the form with the suggestion whose ID is `suggestion_id`.
  // `form_index` corresponds to the vector of ActorFormFillingRequests
  // retrieved by GetSuggestions().
  virtual void PreviewForm(const tabs::TabInterface& tab,
                           int form_index,
                           ActorSuggestionId suggestion_id) = 0;

  // Clears the preview for the form.
  // `form_index` corresponds to the vector of ActorFormFillingRequests
  // retrieved by GetSuggestions().
  virtual void ClearFormPreview(const tabs::TabInterface& tab,
                                int form_index) = 0;

  // Fills the form with the given `selection` .
  // `form_index` corresponds to the vector of ActorFormFillingRequests
  // retrieved by GetSuggestions().
  virtual void FillForm(const tabs::TabInterface& tab,
                        int form_index,
                        ActorFormFillingSelection selection) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_FILLING_SERVICE_H_
