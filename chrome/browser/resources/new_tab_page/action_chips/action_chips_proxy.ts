// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionChipsHandlerInterface} from '../action_chips.mojom-webui.js';
import {ActionChipsHandlerFactory, ActionChipsHandlerRemote, PageCallbackRouter} from '../action_chips.mojom-webui.js';

export interface ActionChipsApiProxy {
  getHandler(): ActionChipsHandlerInterface;
  getCallbackRouter(): PageCallbackRouter;
}

export class ActionChipsApiProxyImpl implements ActionChipsApiProxy {
  private handler: ActionChipsHandlerRemote;
  private callbackRouter: PageCallbackRouter;

  constructor() {
    this.handler = new ActionChipsHandlerRemote();
    this.callbackRouter = new PageCallbackRouter();
    const factory = ActionChipsHandlerFactory.getRemote();
    factory.createActionChipsHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  getHandler(): ActionChipsHandlerInterface {
    return this.handler;
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }

  static getInstance(): ActionChipsApiProxy {
    return instance || (instance = new ActionChipsApiProxyImpl());
  }

  static setInstance(newInstance: ActionChipsApiProxy|null) {
    instance = newInstance;
  }
}

let instance: ActionChipsApiProxy|null = null;
