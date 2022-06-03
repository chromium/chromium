// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_
#define CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_

#include "services/network/public/mojom/network_service.mojom.h"

class PrefService;
class PrefRegistrySimple;

// An interface for sending updated network::mojom::SSLConfigs to one or more
// network::Mojom::SSLConfigClients. Not threadsafe.
class SSLConfigServiceManager {
 public:
  // Create an instance of the SSLConfigServiceManager. The lifetime of the
  // PrefService objects must be longer than that of the manager. Get SSL
  // preferences from local_state object.
  static SSLConfigServiceManager* CreateDefaultManager(
      PrefService* local_state);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual ~SSLConfigServiceManager() {}

  // Populates the SSLConfig-related members of |network_context_params|
  // (|initial_ssl_config| and |ssl_config_client_receiver|). Updated SSLConfigs
  // will be send to the NetworkContext created with those params whenever the
  // configuration changes. Can be called more than once to inform multiple
  // NetworkContexts of changes.
  virtual void AddToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params) = 0;

  // Flushes all SSLConfigClient mojo pipes, to avoid races in tests.
  virtual void FlushForTesting() = 0;
};

#endif  // CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_
