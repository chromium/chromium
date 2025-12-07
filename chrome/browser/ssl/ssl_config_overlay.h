// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CONFIG_OVERLAY_H_
#define CHROME_BROWSER_SSL_SSL_CONFIG_OVERLAY_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

// This object can be instantiated by a configurer of NetworkContexts that wants
// to apply overrides to certain fields in the SSLConfig whose values may
// differ from the global values (e.g. for specific per-Profile settings that do
// not apply globally).
//
// This object is initialized with a NetworkContextParams (containing the
// initial global SSLConfig and a pending SSLConfigClient receiver) obtained
// from SSLConfigServiceManager::AddToNetworkContextParams(). It applies the
// specific override config settings, and inserts itself in front of the
// original SSLConfigClient Mojo pipe to intercept calls. It passes along global
// updates it receives (after applying its overrides), and also updates the
// downstream SSLConfigClient when the specific override settings are updated.
class SSLConfigOverlay : public network::mojom::SSLConfigClient {
 public:
  // Callback that updates the config with the specific override settings.
  using OverrideConfigCallback =
      base::RepeatingCallback<void(network::mojom::SSLConfig*)>;

  explicit SSLConfigOverlay(OverrideConfigCallback callback);
  ~SSLConfigOverlay() override;

  SSLConfigOverlay(const SSLConfigOverlay&) = delete;
  SSLConfigOverlay& operator=(const SSLConfigOverlay&) = delete;

  // Applies the specific overrides to
  // `network_context_params->initial_ssl_config`, and steals the original
  // SSLConfigClient pipe from
  // `network_context_params->ssl_config_client_receiver` so that we can insert
  // ourselves in the middle. May only be called once. Returns whether
  // initialization succeeded at binding both Mojo pipes.
  bool Init(network::mojom::NetworkContextParams* network_context_params);

  // Should be called when the relevant override settings change. Recomputes the
  // overridden config by applying the overrides to the last global config, then
  // notifies the downstream SSLConfigClient.
  void Update();

  // Whether this object is able to relay updates (has both Mojo pipes bound).
  // If this ever becomes unbound after being initialized, it is no longer
  // functional so might as well be cleaned up.
  bool IsBound() const;

  // network::mojom::SSLConfigClient implementation:
  void OnSSLConfigUpdated(network::mojom::SSLConfigPtr ssl_config) override;

  void FlushForTesting();

 private:
  // Applies the override settings to `config` and then notifies the downstream
  // SSLConfigClient.
  void ApplyOverridesAndNotify(network::mojom::SSLConfigPtr config);

  // Resets both Mojo pipes.
  void OnDisconnect();

  // Called to override specific settings on the SSLConfig.
  OverrideConfigCallback override_callback_;
  // Will receive updates about changes to the original (global) config.
  mojo::Receiver<network::mojom::SSLConfigClient> receiver_{this};
  // The downstream client, to be notified on global changes and override
  // changes.
  mojo::Remote<network::mojom::SSLConfigClient> ssl_config_client_;
  // Cached copy of the last global config.
  network::mojom::SSLConfigPtr last_global_config_;
};

#endif  // CHROME_BROWSER_SSL_SSL_CONFIG_OVERLAY_H_
