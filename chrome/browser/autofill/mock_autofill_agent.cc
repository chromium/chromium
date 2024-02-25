// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/mock_autofill_agent.h"

#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill {

MockAutofillAgent::MockAutofillAgent() = default;
MockAutofillAgent::~MockAutofillAgent() = default;

void MockAutofillAgent::BindForTesting(content::RenderFrameHost* rfh) {
  blink::AssociatedInterfaceProvider* remote_interfaces =
      rfh->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::AutofillAgent::Name_,
      base::BindRepeating(&MockAutofillAgent::BindPendingReceiver,
                          weak_ptr_factory_.GetWeakPtr()));
}

void MockAutofillAgent::BindPendingReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::AutofillAgent>(
                           std::move(handle)));
}

}  // namespace autofill
