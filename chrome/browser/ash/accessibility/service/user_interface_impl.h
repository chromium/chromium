// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_USER_INTERFACE_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_USER_INTERFACE_IMPL_H_

#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/accessibility/public/mojom/assistive_technology_type.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"

namespace ash {

// The UserInterfaceImpl receives text-to-speech requests from the Accessibility
// Service and updates the Chrome OS user interface.
class UserInterfaceImpl : public ax::mojom::UserInterface {
 public:
  UserInterfaceImpl();
  UserInterfaceImpl(const UserInterfaceImpl&) = delete;
  UserInterfaceImpl& operator=(const UserInterfaceImpl&) = delete;
  ~UserInterfaceImpl() override;

  void Bind(mojo::PendingReceiver<ax::mojom::UserInterface> ui_receiver);

  // ax::mojom::UserInterface:
  void SetFocusRings(std::vector<ax::mojom::FocusRingInfoPtr> focus_rings,
                     ax::mojom::AssistiveTechnologyType at_type) override;

 private:
  mojo::ReceiverSet<ax::mojom::UserInterface> ui_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_USER_INTERFACE_IMPL_H_
