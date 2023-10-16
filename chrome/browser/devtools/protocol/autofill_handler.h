// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/devtools/protocol/autofill.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/browser/web_contents.h"

using protocol::Maybe;
using protocol::String;

namespace autofill {
class ContentAutofillDriver;
class AutofillProfile;
class CreditCard;
}

class AutofillHandler : public protocol::Autofill::Backend,
                        autofill::AutofillManager::Observer {
 public:
  AutofillHandler(protocol::UberDispatcher* dispatcher,
                  const std::string& target_id);

  AutofillHandler(const AutofillHandler&) = delete;
  AutofillHandler& operator=(const AutofillHandler&) = delete;

  ~AutofillHandler() override;

 private:
  protocol::Response Enable() override;
  protocol::Response Disable() override;
  void Trigger(int field_id,
               Maybe<String> frame_id,
               std::unique_ptr<protocol::Autofill::CreditCard> card,
               std::unique_ptr<TriggerCallback> callback) override;
  void FinishTrigger(Maybe<String> frame_id,
                     std::unique_ptr<protocol::Autofill::CreditCard> card,
                     std::unique_ptr<TriggerCallback> callback,
                     uint64_t field_id);
  // Sets a list of addresses inside `AutofillManager`, used to provide
  // developers addresses from different countries so that they can be used for
  // testing their form.
  void SetAddresses(
      std::unique_ptr<protocol::Array<protocol::Autofill::Address>> addresses,
      std::unique_ptr<SetAddressesCallback> callback) override;

  // Autofill::AutofillManagerObserver
  // Observes form filled events. In the case of an address form, we emit to
  // devtools the filled fields details and information about the profile used.
  // These information is then used to build a UI inside devtools, which will
  // provide developers more visibility on how autofill works on their form.
  void OnFillOrPreviewDataModelForm(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form,
      autofill::mojom::ActionPersistence action_persistence,
      base::span<const autofill::FormFieldData* const> filled_fields,
      absl::variant<const autofill::AutofillProfile*,
                    const autofill::CreditCard*> profile_or_credit_card)
      override;
  // Called when the manager is destroyed and used to reset the observer.
  void OnAutofillManagerDestroyed(autofill::AutofillManager& manager) override;

  // Returns the driver for the outermost frame, not the one that created the
  // `DevToolsAgentHost` and iniated the session.
  autofill::ContentAutofillDriver* GetAutofillDriver();

  const std::string target_id_;
  bool enabled_ = false;
  std::unique_ptr<protocol::Autofill::Frontend> frontend_;
  base::ScopedObservation<autofill::AutofillManager,
                          autofill::AutofillManager::Observer>
      observation_{this};
  base::WeakPtrFactory<AutofillHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_
