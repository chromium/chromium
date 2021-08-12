// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommandHandlerFactory, CommandHandlerRemote} from './promo_browser_command.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending the What's New browser commands to the browser and
 * receiving the browser response.
 */

let instance: WhatsNewCommandProxy|null = null;

export class WhatsNewCommandProxy {
  handler: CommandHandlerRemote;

  /** @return {!WhatsNewCommandProxy} */
  static getInstance(): WhatsNewCommandProxy {
    return instance || (instance = new WhatsNewCommandProxy());
  }

  static setInstance(newInstance: WhatsNewCommandProxy) {
    instance = newInstance;
  }

  constructor() {
    this.handler = new CommandHandlerRemote();
    const factory = CommandHandlerFactory.getRemote();
    factory.createBrowserCommandHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}
