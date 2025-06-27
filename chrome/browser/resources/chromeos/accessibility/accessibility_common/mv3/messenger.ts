// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OffscreenCommandType} from './offscreen_command_type.js';

/*
 * Helper to wrap messaging api between offscreen doc and service worker.
 */
export class Messenger {
  static instance?: Messenger;

  // Tracks registered message handlers.
  private registry_: Map<OffscreenCommandType, Messenger.Handler>;

  constructor() {
    this.registry_ = new Map<OffscreenCommandType, Messenger.Handler>();

    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: chrome.runtime.MessageSender,
         sendResponse: (response?: any) => void) =>
            this.handleMessage_(message, sendResponse));
  }

  static init(): void {
    if (Messenger.instance) {
      throw 'Error: trying to create two instances of singleton Messenger.';
    }
    Messenger.instance = new Messenger();
  }

  // Registers a command handler.
  static registerHandler(
      command: OffscreenCommandType, handler: Messenger.Handler) {
    Messenger.instance!.registry_.set(command, handler);
  }

  // Gets the handler for a given command.
  static getHandler(command: OffscreenCommandType): Messenger.Handler
      |undefined {
    return Messenger.instance!.registry_.get(command);
  }

  // Handles the command message received from the other parts of the extension.
  // For example, when running in the service worker, this handles messages from
  // the offscreen document. And when running in the offscreen document, it
  // handles messages from the service worker.
  private handleMessage_(
      message: any|undefined, sendResponse: (response?: any) => void): boolean {
    const command = message['command'];
    const result = Messenger.getHandler(command)?.(message);

    // If handler is async, return true to allow async sendResponse.
    if (result instanceof Promise) {
      result.then(sendResponse).catch(sendResponse);
      return true;
    }
    // Otherwise return false.
    return false;
  }
}

export namespace Messenger {
  // Message handler. If a handler returns a Promise, a reply will be sent
  // to the sender when the promise resolves.
  export type Handler = (message: any|undefined) => Promise<any>|void;
}
