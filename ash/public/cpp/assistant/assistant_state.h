// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_H_

#include <string>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

// Interface for a class that holds Assistant related prefs and states.
class ASH_PUBLIC_EXPORT AssistantState
    : public AssistantStateBase,
      public mojom::AssistantStateController {
 public:
  static AssistantState* Get();

  AssistantState();
  ~AssistantState() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantStateController> receiver);
  void NotifyStatusChanged(mojom::AssistantState state);
  void NotifyFeatureAllowed(mojom::AssistantAllowedState state);
  void NotifyLocaleChanged(const std::string& locale);
  void NotifyArcPlayStoreEnabledChanged(bool enabled);
  void NotifyLockedFullScreenStateChanged(bool enabled);

  // ash::mojom::AssistantStateController:
  void AddMojomObserver(mojo::PendingRemote<mojom::AssistantStateObserver>
                            pending_observer) override;

 private:
  mojo::ReceiverSet<mojom::AssistantStateController> receivers_;

  mojo::RemoteSet<mojom::AssistantStateObserver> remote_observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantState);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_H_
