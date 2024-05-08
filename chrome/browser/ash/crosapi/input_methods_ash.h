// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_INPUT_METHODS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_INPUT_METHODS_ASH_H_

#include <string>

#include "chromeos/crosapi/mojom/input_methods.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the `InputMethods` crosapi interface.
class InputMethodsAsh : public mojom::InputMethods {
 public:
  InputMethodsAsh();
  InputMethodsAsh(const InputMethodsAsh&) = delete;
  InputMethodsAsh& operator=(const InputMethodsAsh&) = delete;
  InputMethodsAsh(InputMethodsAsh&&) = delete;
  InputMethodsAsh& operator=(InputMethodsAsh&&) = delete;
  ~InputMethodsAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::InputMethods> receiver);

  // `crosapi::mojom::InputMethods` implementation:
  void ChangeInputMethod(const std::string& input_method_id,
                         ChangeInputMethodCallback callback) override;

 private:
  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::InputMethods> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_INPUT_METHODS_ASH_H_
