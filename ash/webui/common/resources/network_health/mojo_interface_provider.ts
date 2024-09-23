// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkDiagnosticsRoutines, NetworkDiagnosticsRoutinesInterface} from '//resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

/**
 * @fileoverview
 * This file contains the mojo interface for the network diagnostics service and
 * methods to override the service for testing.
 */

let networkDiagnosticsService: NetworkDiagnosticsRoutinesInterface|null = null;

export function setNetworkDiagnosticsServiceForTesting(
    testNetworkDiagnosticsService: NetworkDiagnosticsRoutinesInterface) {
  networkDiagnosticsService = testNetworkDiagnosticsService;
}

export function getNetworkDiagnosticsService():
    NetworkDiagnosticsRoutinesInterface {
  if (networkDiagnosticsService) {
    return networkDiagnosticsService;
  }

  networkDiagnosticsService = NetworkDiagnosticsRoutines.getRemote();
  return networkDiagnosticsService;
}
