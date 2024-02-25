// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_USER_INPUT_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_USER_INPUT_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/accessibility/public/mojom/user_input.mojom.h"

namespace ash {

// The UserInputImpl performs actions for an accessibility service, such as
// sending synthetic events or performing an accelerator action.
class UserInputImpl : public ax::mojom::UserInput {
 public:
  UserInputImpl();
  UserInputImpl(const UserInputImpl&) = delete;
  UserInputImpl& operator=(const UserInputImpl&) = delete;
  ~UserInputImpl() override;

  void Bind(mojo::PendingReceiver<ax::mojom::UserInput> ui_receiver);

  // ax::mojom::UserInput:

  // Synthetic key events are only used for simulated keyboard navigation, and
  // do not support internationalization. Any text entry should be done via IME.
  void SendSyntheticKeyEventForShortcutOrNavigation(
      ax::mojom::SyntheticKeyEventPtr key_event) override;

  void SendSyntheticMouseEvent(
      ax::mojom::SyntheticMouseEventPtr mouse_event) override;

 private:
  mojo::ReceiverSet<ax::mojom::UserInput> ui_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_USER_INPUT_IMPL_H_
