// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_config_overlay.h"

#include "services/network/public/mojom/network_context.mojom.h"

SSLConfigOverlay::SSLConfigOverlay(OverrideConfigCallback callback)
    : override_callback_(std::move(callback)) {
  CHECK(override_callback_);
}

SSLConfigOverlay::~SSLConfigOverlay() = default;

bool SSLConfigOverlay::Init(
    network::mojom::NetworkContextParams* network_context_params) {
  CHECK(!last_global_config_);
  CHECK(!receiver_.is_bound());
  CHECK(!ssl_config_client_.is_bound());

  if (network_context_params->initial_ssl_config) {
    last_global_config_ = network_context_params->initial_ssl_config->Clone();
    override_callback_.Run(network_context_params->initial_ssl_config.get());
  }
  if (network_context_params->ssl_config_client_receiver) {
    receiver_.Bind(
        std::move(network_context_params->ssl_config_client_receiver));
    network_context_params->ssl_config_client_receiver =
        ssl_config_client_.BindNewPipeAndPassReceiver();

    // Propagate a disconnect on either end of the pipe to the other end.
    receiver_.set_disconnect_handler(base::BindOnce(
        &SSLConfigOverlay::OnDisconnect, base::Unretained(this)));
    ssl_config_client_.set_disconnect_handler(base::BindOnce(
        &SSLConfigOverlay::OnDisconnect, base::Unretained(this)));
  }
  return IsBound();
}

void SSLConfigOverlay::Update() {
  ApplyOverridesAndNotify(last_global_config_.Clone());
}

bool SSLConfigOverlay::IsBound() const {
  return ssl_config_client_.is_bound() && receiver_.is_bound();
}

void SSLConfigOverlay::OnSSLConfigUpdated(
    network::mojom::SSLConfigPtr ssl_config) {
  last_global_config_ = ssl_config->Clone();
  ApplyOverridesAndNotify(std::move(ssl_config));
}

void SSLConfigOverlay::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
  if (ssl_config_client_) {
    ssl_config_client_.FlushForTesting();  // IN-TEST
  }
}

void SSLConfigOverlay::ApplyOverridesAndNotify(
    network::mojom::SSLConfigPtr config) {
  override_callback_.Run(config.get());
  if (ssl_config_client_.is_bound()) {
    ssl_config_client_->OnSSLConfigUpdated(std::move(config));
  }
}

void SSLConfigOverlay::OnDisconnect() {
  ssl_config_client_.reset();
  receiver_.reset();
}
