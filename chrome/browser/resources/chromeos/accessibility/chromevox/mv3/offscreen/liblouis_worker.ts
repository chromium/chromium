// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BridgeHelper} from '/common/bridge_helper.js';

import {BackgroundBridge} from '../common/background_bridge.js';
import {BridgeConstants} from '../common/bridge_constants.js';

const TARGET = BridgeConstants.Offscreen.TARGET;
const Action = BridgeConstants.Offscreen.Action;

export class LibLouisWorker {
  private worker_: Worker|null = null;

  static instance?: LibLouisWorker;

  constructor() {
    BridgeHelper.registerHandler(
        TARGET, Action.LIBLOUIS_START_WORKER,
        (wasmPath: string) => this.startWorker_(wasmPath));
    BridgeHelper.registerHandler(
        TARGET, Action.LIBLOUIS_RPC,
        (messageJson: string) => this.postMessage_(messageJson));
  }

  private postMessage_(messageJson: string): {message?: string} {
    if (!this.worker_) {
      return {message: 'Cannot send RPC: liblouis worker not started.'};
    }
    this.worker_.postMessage(messageJson);
    return {};
  }

  static init(): void {
    if (LibLouisWorker.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'LibLouisWorker.';
    }
    LibLouisWorker.instance = new LibLouisWorker();
  }

  private startWorker_(wasmPath: string): void {
    this.worker_ = new Worker(wasmPath);
    this.worker_.addEventListener(
        'message', e => this.onInstanceMessage_(e), false /* useCapture */);
    this.worker_.addEventListener(
        'error', e => this.onInstanceError_(e), false /* useCapture */);
  }

  private onInstanceMessage_(e: MessageEvent): void {
    BackgroundBridge.LibLouis.message(e.data);
  }

  private onInstanceError_(e: ErrorEvent): void {
    BackgroundBridge.LibLouis.error(e.message);
  }
}
