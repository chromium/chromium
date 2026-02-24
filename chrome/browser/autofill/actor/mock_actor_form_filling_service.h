// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_MOCK_ACTOR_FORM_FILLING_SERVICE_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_MOCK_ACTOR_FORM_FILLING_SERVICE_H_

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/actor_form_filling_service.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockActorFormFillingService : public ActorFormFillingService {
 public:
  MockActorFormFillingService();
  ~MockActorFormFillingService() override;

  MOCK_METHOD(void,
              GetSuggestions,
              (const tabs::TabInterface& tab,
               base::span<const FillRequest> fill_requests,
               base::OnceCallback<
                   void(base::expected<std::vector<ActorFormFillingRequest>,
                                       ActorFormFillingError>)>),
              (override));

  MOCK_METHOD(
      void,
      FillSuggestions,
      (const tabs::TabInterface& tab,
       base::span<const ActorFormFillingSelection> chosen_suggestions,
       base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>),
      (override));

  MOCK_METHOD(void, ScrollToForm, (const tabs::TabInterface&, int), (override));
  MOCK_METHOD(void,
              PreviewForm,
              (const tabs::TabInterface&, int, ActorSuggestionId),
              (override));
  MOCK_METHOD(void,
              ClearFormPreview,
              (const tabs::TabInterface&, int),
              (override));
  MOCK_METHOD(void,
              FillForm,
              (const tabs::TabInterface&, int, ActorFormFillingSelection),
              (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_MOCK_ACTOR_FORM_FILLING_SERVICE_H_
