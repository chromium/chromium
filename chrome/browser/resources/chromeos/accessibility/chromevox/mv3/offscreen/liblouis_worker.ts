// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OffscreenCommandType} from '../common/offscreen_command_type.js';

type MessageSender = chrome.runtime.MessageSender;
type SendResponse = (value: any) => void;

export class LibLouisWorker {
  private worker_: Worker|null = null;

  static instance?: LibLouisWorker;

  constructor() {
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: MessageSender,
         sendResponse: SendResponse) =>
            this.handleMessageFromServiceWorker_(message, sendResponse));
  }

  private handleMessageFromServiceWorker_(
      message: any|undefined, sendResponse: SendResponse): boolean {
    switch (message['command']) {
      case OffscreenCommandType.LIBLOUIS_START_WORKER:
        this.startWorker_(message['wasmPath']);
        break;
      case OffscreenCommandType.LIBLOUIS_RPC:
        if (!this.worker_) {
          sendResponse(
              {message: 'Cannot send RPC: liblouis worker not started.'});
        } else {
          this.worker_.postMessage(message['messageJson']);
          // No error.
          sendResponse({});
        }
        break;
    }
    return false;
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
    chrome.runtime.sendMessage(
        undefined,
        {command: OffscreenCommandType.LIBLOUIS_MESSAGE, data: e.data});
  }

  private onInstanceError_(e: ErrorEvent): void {
    chrome.runtime.sendMessage(
        undefined,
        {command: OffscreenCommandType.LIBLOUIS_ERROR, message: e.message});
  }
}
