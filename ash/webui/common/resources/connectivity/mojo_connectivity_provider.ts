// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PasspointService, PasspointServiceInterface} from './passpoint.mojom-webui.js';

/**
 * @fileoverview Wrapper for connectivity services that provides the ability to
 * inject a fake implementation for tests.
 */

export class MojoConnectivityProvider {
  private static instance: MojoConnectivityProvider|null = null;
  private passpointService: PasspointServiceInterface|null = null;

  getPasspointService(): PasspointServiceInterface {
    if (!this.passpointService) {
      this.passpointService = PasspointService.getRemote();
    }
    return this.passpointService;
  }

  setPasspointServiceForTest(service: PasspointServiceInterface): void {
    this.passpointService = service;
  }

  static getInstance(): MojoConnectivityProvider {
    if (!this.instance) {
      this.instance = new MojoConnectivityProvider();
    }
    return this.instance;
  }
}
