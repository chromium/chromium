// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug/1127165, crbug/1179821): Remove these Mojo lite bindings once the
// Tast test JS fixtures are converted from a hardcoded string to a file and
// the global chromeos.networkDiagnostics.mojom namespace is avoided. In the
// meantime, these lite bindings are required to expose the namespace.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-lite.js';

import {NetworkDiagnosticsRoutines, NetworkDiagnosticsRoutinesInterface} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

/**
 * @fileoverview
 * This file contains the mojo interface for the network diagnostics service and
 * methods to override the service for testing.
 */

/**
 * @type {?NetworkDiagnosticsRoutinesInterface}
 */
let networkDiagnosticsService = null;

/**
 * @param {!NetworkDiagnosticsRoutinesInterface} testNetworkDiagnosticsService
 */
export function setNetworkDiagnosticsServiceForTesting(
    testNetworkDiagnosticsService) {
  networkDiagnosticsService = testNetworkDiagnosticsService;
}

/**
 * @return {!NetworkDiagnosticsRoutinesInterface}
 */
export function getNetworkDiagnosticsService() {
  if (networkDiagnosticsService) {
    return networkDiagnosticsService;
  }

  networkDiagnosticsService = NetworkDiagnosticsRoutines.getRemote();
  return networkDiagnosticsService;
}
