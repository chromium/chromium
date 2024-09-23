// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/input_methods_ash.h"

#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/crosapi/mojom/input_methods.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace crosapi {

using ash::input_method::InputMethodManager;

InputMethodsAsh::InputMethodsAsh() = default;
InputMethodsAsh::~InputMethodsAsh() = default;

void InputMethodsAsh::BindReceiver(
    mojo::PendingReceiver<mojom::InputMethods> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void InputMethodsAsh::ChangeInputMethod(const std::string& input_method_id,
                                        ChangeInputMethodCallback callback) {
  auto& input_method_manager = CHECK_DEREF(InputMethodManager::Get());
  InputMethodManager::State& ime_state =
      CHECK_DEREF(input_method_manager.GetActiveIMEState().get());

  const std::string migrated_input_method_id =
      input_method_manager.GetMigratedInputMethodID(input_method_id);

  const bool is_input_method_enabled = base::Contains(
      ime_state.GetEnabledInputMethodIds(), migrated_input_method_id);

  if (is_input_method_enabled) {
    ime_state.ChangeInputMethod(migrated_input_method_id,
                                /*show_message=*/false);

    std::move(callback).Run(/*succeeded=*/true);
  } else {
    std::move(callback).Run(/*succeeded=*/false);
  }
}

}  // namespace crosapi
