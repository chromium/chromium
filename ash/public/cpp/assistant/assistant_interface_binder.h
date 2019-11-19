// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_INTERFACE_BINDER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_INTERFACE_BINDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

// An interface for binding Ash-provided client APIs needed by the Assistant
// service.
class ASH_PUBLIC_EXPORT AssistantInterfaceBinder {
 public:
  static AssistantInterfaceBinder* GetInstance();
  static void SetInstance(AssistantInterfaceBinder* binder);

  virtual void BindController(
      mojo::PendingReceiver<chromeos::assistant::mojom::AssistantController>
          receiver) = 0;
  virtual void BindAlarmTimerController(
      mojo::PendingReceiver<mojom::AssistantAlarmTimerController> receiver) = 0;
  virtual void BindNotificationController(
      mojo::PendingReceiver<mojom::AssistantNotificationController>
          receiver) = 0;
  virtual void BindScreenContextController(
      mojo::PendingReceiver<mojom::AssistantScreenContextController>
          receiver) = 0;
  virtual void BindStateController(
      mojo::PendingReceiver<mojom::AssistantStateController> receiver) = 0;
  virtual void BindVolumeControl(
      mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) = 0;

 protected:
  AssistantInterfaceBinder();
  virtual ~AssistantInterfaceBinder();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_INTERFACE_BINDER_H_
