// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_AGENT_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class RenderFrameHost;
}

namespace autofill {

class MockAutofillAgent : public mojom::AutofillAgent {
 public:
  MockAutofillAgent();
  MockAutofillAgent(const MockAutofillAgent&) = delete;
  MockAutofillAgent& operator=(const MockAutofillAgent&) = delete;
  ~MockAutofillAgent() override;

  void BindForTesting(content::RenderFrameHost* rfh);
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  MOCK_METHOD(void, TriggerFormExtraction, (), (override));
  MOCK_METHOD(void,
              TriggerFormExtractionWithResponse,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(
      void,
      ExtractForm,
      (FormRendererId form,
       base::OnceCallback<void(const std::optional<FormData>&)> callback),
      (override));
  MOCK_METHOD(void,
              ApplyFieldsAction,
              (mojom::FormActionType action_type,
               mojom::ActionPersistence action_persistence,
               const std::vector<FormFieldData::FillData>& fields),
              (override));
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType action_type,
               mojom::ActionPersistence action_persistence,
               FieldRendererId field,
               const std::u16string& value),
              (override));
  MOCK_METHOD(void,
              FieldTypePredictionsAvailable,
              (const std::vector<FormDataPredictions>& forms),
              (override));
  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              TriggerSuggestions,
              (FieldRendererId field_id,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              SetSuggestionAvailability,
              (FieldRendererId field,
               mojom::AutofillSuggestionAvailability suggestion_availability),
              (override));
  MOCK_METHOD(void,
              AcceptDataListSuggestion,
              (FieldRendererId field, const ::std::u16string& value),
              (override));
  MOCK_METHOD(void,
              PreviewPasswordSuggestion,
              (const ::std::u16string& username,
               const ::std::u16string& password),
              (override));
  MOCK_METHOD(void,
              PreviewPasswordGenerationSuggestion,
              (const ::std::u16string& password),
              (override));
  MOCK_METHOD(void,
              GetPotentialLastFourCombinationsForStandaloneCvc,
              (base::OnceCallback<void(const std::vector<std::string>&)>),
              (override));

 private:
  mojo::AssociatedReceiverSet<mojom::AutofillAgent> receivers_;
  base::WeakPtrFactory<MockAutofillAgent> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_AGENT_H_
