// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionChipsHandlerInterface} from '../action_chips.mojom-webui.js';
import {ActionChipsHandlerFactory, ActionChipsHandlerRemote} from '../action_chips.mojom-webui.js';

export interface ActionChipsApiProxy {
  getHandler(): ActionChipsHandlerInterface;
}

export class ActionChipsApiProxyImpl implements ActionChipsApiProxy {
  private handler: ActionChipsHandlerRemote;

  constructor() {
    this.handler = new ActionChipsHandlerRemote();
    const factory = ActionChipsHandlerFactory.getRemote();
    factory.createActionChipsHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getHandler(): ActionChipsHandlerInterface {
    return this.handler;
  }

  static getInstance(): ActionChipsApiProxy {
    return instance || (instance = new ActionChipsApiProxyImpl());
  }

  static setInstance(newInstance: ActionChipsApiProxy|null) {
    instance = newInstance;
  }
}

let instance: ActionChipsApiProxy|null = null;
