// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {MicrosoftAuthPageHandlerRemote} from '../../../microsoft_auth.mojom-webui.js';
import {MicrosoftAuthPageHandler} from '../../../microsoft_auth.mojom-webui.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the page and
 * the browser.
 */

export interface MicrosoftAuthProxy {
  handler: MicrosoftAuthPageHandlerRemote;
}

export class MicrosoftAuthProxyImpl implements MicrosoftAuthProxy {
  handler: MicrosoftAuthPageHandlerRemote;

  constructor(handler: MicrosoftAuthPageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): MicrosoftAuthProxy {
    if (instance) {
      return instance;
    }

    const handler = MicrosoftAuthPageHandler.getRemote();
    return instance = new MicrosoftAuthProxyImpl(handler);
  }

  static setInstance(obj: MicrosoftAuthProxy) {
    instance = obj;
  }
}

let instance: MicrosoftAuthProxy|null = null;
