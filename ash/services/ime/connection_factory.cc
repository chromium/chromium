// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/connection_factory.h"

#include <utility>

namespace ash {
namespace ime {

ConnectionFactory::ConnectionFactory(
    mojo::PendingReceiver<mojom::ConnectionFactory> pending_receiver)
    : receiver_(this, std::move(pending_receiver)) {}

ConnectionFactory::~ConnectionFactory() = default;

void ConnectionFactory::ConnectToInputMethod(
    const std::string& ime_spec,
    mojo::PendingAssociatedReceiver<mojom::InputMethod> pending_input_method,
    mojo::PendingAssociatedRemote<mojom::InputMethodHost>
        pending_input_method_host,
    mojom::InputMethodSettingsPtr settings,
    ConnectToInputMethodCallback callback) {
  // `settings` is unused because Rule-Based IMEs do not have settings.
  rule_based_engine_ =
      RuleBasedEngine::Create(ime_spec, std::move(pending_input_method),
                              std::move(pending_input_method_host));
  std::move(callback).Run(/*bound=*/true);
}

bool ConnectionFactory::IsConnected() {
  return rule_based_engine_ && rule_based_engine_->IsConnected();
}

}  // namespace ime
}  // namespace ash
