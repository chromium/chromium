// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_CONNECTION_FACTORY_H_
#define ASH_SERVICES_IME_CONNECTION_FACTORY_H_

#include <memory>
#include <string>

#include "ash/services/ime/associated_rule_based_engine.h"
#include "ash/services/ime/public/mojom/connection_factory.mojom.h"
#include "ash/services/ime/public/mojom/input_method.mojom.h"
#include "ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
namespace ime {

// Chromium implementation of ConnectionFactory (as opposed to the sharedlib
// implementation). This impl is used to connect the RuleBasedEngine in the ime
// service to the NativeIME when requested.
class ConnectionFactory : public mojom::ConnectionFactory {
 public:
  explicit ConnectionFactory(
      mojo::PendingReceiver<mojom::ConnectionFactory> pending_receiver);
  ~ConnectionFactory() override;

  // mojom::ConnectionFactory overrides.
  void ConnectToInputMethod(
      const std::string& ime_spec,
      mojo::PendingAssociatedReceiver<mojom::InputMethod> pending_input_method,
      mojo::PendingAssociatedRemote<mojom::InputMethodHost>
          pending_input_method_host,
      ConnectToInputMethodCallback callback) override;

  // Is the current connection factory connected to a rule based engine?
  bool IsConnected();

 private:
  mojo::Receiver<mojom::ConnectionFactory> receiver_;

  // This connection factory is only ever used to connect to a rule based
  // engine.
  std::unique_ptr<AssociatedRuleBasedEngine> rule_based_engine_;
};

}  // namespace ime
}  // namespace ash

#endif  // ASH_SERVICES_IME_CONNECTION_FACTORY_H_
