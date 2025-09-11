// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/devtools/protocol/autofill.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "content/public/browser/web_contents.h"

using protocol::String;

namespace autofill {
class AutofillClient;
class ContentAutofillDriver;
class CreditCard;
}

class AutofillHandler : public protocol::Autofill::Backend,
                        autofill::AutofillManager::Observer,
                        autofill::ContentAutofillDriverFactory::Observer {
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
               std::optional<String> frame_id,
               std::unique_ptr<protocol::Autofill::CreditCard> card,
               std::unique_ptr<protocol::Autofill::Address> address,
               std::unique_ptr<TriggerCallback> callback) override;

  // Called after form extraction is finished in all frames.
  void ContinueTrigger(content::RenderFrameHost* frame_rfh,
                       int field_id,
                       std::optional<String> frame_id,
                       std::unique_ptr<protocol::Autofill::CreditCard> card,
                       std::unique_ptr<protocol::Autofill::Address> address,
                       std::unique_ptr<TriggerCallback> callback,
                       bool success);

  // Sets a list of addresses inside `AutofillManager`, used to provide
  // developers addresses from different countries so that they can be used for
  // testing their form.
  void SetAddresses(
      std::unique_ptr<protocol::Array<protocol::Autofill::Address>> addresses,
      std::unique_ptr<SetAddressesCallback> callback) override;

  // AutofillManager::Observer:
  // Observes form filled events. In the case of an address form, we emit to
  // devtools the filled fields details and information about the profile used.
  // These information is then used to build a UI inside devtools, which will
  // provide developers more visibility on how autofill works on their form.
  void OnAutofillManagerStateChanged(
      autofill::AutofillManager& manager,
      autofill::AutofillManager::LifecycleState old_state,
      autofill::AutofillManager::LifecycleState new_state) override;
  void OnFillOrPreviewForm(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form_id,
      autofill::mojom::ActionPersistence action_persistence,
      const base::flat_set<autofill::FieldGlobalId>& filled_field_ids,
      const autofill::FillingPayload& filling_payload) override;

  // ContentAutofillDriverFactory::Observer:
  void OnContentAutofillDriverFactoryDestroyed(
      autofill::ContentAutofillDriverFactory& factory) override;
  void OnContentAutofillDriverCreated(
      autofill::ContentAutofillDriverFactory&,
      autofill::ContentAutofillDriver&) override;

  // Returns the driver for the outermost frame, not the one that created the
  // `DevToolsAgentHost` and initiated the session.
  autofill::ContentAutofillDriver* GetAutofillDriver();

  // Returns the client for the webcontents/tab where the devtools window is
  // open.
  autofill::AutofillClient* GetAutofillClient();

  const std::string target_id_;
  bool enabled_ = false;
  std::unique_ptr<protocol::Autofill::Frontend> frontend_;

  // Observes `AutofillManager` associated with the outermost `RenderFrameHost`.
  base::ScopedObservation<autofill::AutofillManager,
                          autofill::AutofillManager::Observer>
      autofill_manager_observation_{this};

  // Observes the factory to keep the `AutofillManager` observation relevant.
  base::ScopedObservation<autofill::ContentAutofillDriverFactory,
                          autofill::ContentAutofillDriverFactory::Observer>
      factory_observation_{this};
  base::WeakPtrFactory<AutofillHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_
