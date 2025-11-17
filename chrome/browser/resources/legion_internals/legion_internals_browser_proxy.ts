// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LegionInternalsPageHandlerRemote, LegionResponseMojoType} from './legion_internals.mojom-webui.js';
import {LegionInternalsPageHandler} from './legion_internals.mojom-webui.js';

/**
 * @fileoverview A browser proxy for the Legion Internals page.
 */

export interface LegionInternalsBrowserProxy {
  connect(url: string, apiKey: string): Promise<void>;
  close(): Promise<void>;
  sendRequest(featureName: string, request: string):
      Promise<LegionResponseMojoType>;
}

export class LegionInternalsBrowserProxyImpl implements
    LegionInternalsBrowserProxy {
  handler: LegionInternalsPageHandlerRemote;

  constructor(handler: LegionInternalsPageHandlerRemote) {
    this.handler = handler;
  }

  connect(url: string, apiKey: string): Promise<void> {
    return this.handler.connect(url, apiKey);
  }

  close(): Promise<void> {
    return this.handler.close();
  }

  async sendRequest(featureName: string, request: string):
      Promise<LegionResponseMojoType> {
    const {response} = await this.handler.sendRequest(featureName, request);
    return response;
  }

  static getInstance(): LegionInternalsBrowserProxy {
    return instance ||
        (instance = new LegionInternalsBrowserProxyImpl(
             LegionInternalsPageHandler.getRemote()));
  }

  static setInstance(newInstance: LegionInternalsBrowserProxy) {
    instance = newInstance;
  }
}

let instance: LegionInternalsBrowserProxy|null = null;
