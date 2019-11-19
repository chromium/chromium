// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mojo_interface_factory.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/display/cros_display_config.h"
#include "ash/ime/ime_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/tray_action/tray_action.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {
namespace mojo_interface_factory {
namespace {

base::LazyInstance<RegisterInterfacesCallback>::Leaky
    g_register_interfaces_callback = LAZY_INSTANCE_INITIALIZER;

void BindCrosDisplayConfigControllerReceiverOnMainThread(
    mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver) {
  if (Shell::HasInstance())
    Shell::Get()->cros_display_config()->BindReceiver(std::move(receiver));
}

void BindImeControllerReceiverOnMainThread(
    mojo::PendingReceiver<mojom::ImeController> receiver) {
  if (Shell::HasInstance())
    Shell::Get()->ime_controller()->BindReceiver(std::move(receiver));
}

void BindTrayActionReceiverOnMainThread(
    mojo::PendingReceiver<mojom::TrayAction> receiver) {
  if (Shell::HasInstance())
    Shell::Get()->tray_action()->BindReceiver(std::move(receiver));
}

}  // namespace

void RegisterInterfaces(
    service_manager::BinderRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(
      base::BindRepeating(&BindCrosDisplayConfigControllerReceiverOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindImeControllerReceiverOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindTrayActionReceiverOnMainThread),
      main_thread_task_runner);

  // Inject additional optional interfaces.
  if (g_register_interfaces_callback.Get()) {
    std::move(g_register_interfaces_callback.Get())
        .Run(registry, main_thread_task_runner);
  }
}

void SetRegisterInterfacesCallback(RegisterInterfacesCallback callback) {
  g_register_interfaces_callback.Get() = std::move(callback);
}

}  // namespace mojo_interface_factory

}  // namespace ash
