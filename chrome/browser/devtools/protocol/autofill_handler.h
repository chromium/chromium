// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_

#include "chrome/browser/devtools/protocol/autofill.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "content/public/browser/web_contents.h"

using protocol::Maybe;
using protocol::String;

namespace autofill {
class ContentAutofillDriver;
}

class AutofillHandler : public protocol::Autofill::Backend {
 public:
  AutofillHandler(protocol::UberDispatcher* dispatcher,
                  const std::string& target_id);

  AutofillHandler(const AutofillHandler&) = delete;
  AutofillHandler& operator=(const AutofillHandler&) = delete;

  ~AutofillHandler() override;

 private:
  void Trigger(int field_id,
               Maybe<String> frame_id,
               std::unique_ptr<protocol::Autofill::CreditCard> card,
               std::unique_ptr<TriggerCallback> callback) override;
  void FinishTrigger(Maybe<String> frame_id,
                     std::unique_ptr<protocol::Autofill::CreditCard> card,
                     std::unique_ptr<TriggerCallback> callback,
                     uint64_t field_id);
  void SetAddresses(
      std::unique_ptr<protocol::Array<protocol::Autofill::Address>> addresses,
      std::unique_ptr<SetAddressesCallback> callback) override;

  // Returns the driver for the outermost frame, not the one that created the
  // `DevToolsAgentHost` and iniated the session.
  autofill::ContentAutofillDriver* GetAutofillDriver();

  const std::string target_id_;
  base::WeakPtrFactory<AutofillHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_AUTOFILL_HANDLER_H_
