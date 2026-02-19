// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LegionInternalsPageHandlerRemote, LegionResponseMojoType} from './legion_internals.mojom-webui.js';
import {LegionInternalsPageCallbackRouter, LegionInternalsPageHandler} from './legion_internals.mojom-webui.js';

/**
 * @fileoverview A browser proxy for the Legion Internals page.
 */

export interface LegionInternalsBrowserProxy {
  connect(
      url: string, apiKey: string, proxyUrl: string,
      useTokenAttestation: boolean): Promise<void>;
  close(): Promise<void>;
  sendRequest(featureName: string, request: string):
      Promise<LegionResponseMojoType>;
  getCallbackRouter(): LegionInternalsPageCallbackRouter;
}

export class LegionInternalsBrowserProxyImpl implements
    LegionInternalsBrowserProxy {
  handler: LegionInternalsPageHandlerRemote;
  callbackRouter: LegionInternalsPageCallbackRouter;

  constructor(handler: LegionInternalsPageHandlerRemote) {
    this.handler = handler;
    this.callbackRouter = new LegionInternalsPageCallbackRouter();

    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  connect(
      url: string, apiKey: string, proxyUrl: string,
      useTokenAttestation: boolean): Promise<void> {
    return this.handler.connect(url, apiKey, proxyUrl, useTokenAttestation);
  }

  close(): Promise<void> {
    return this.handler.close();
  }

  async sendRequest(featureName: string, request: string):
      Promise<LegionResponseMojoType> {
    const {response} = await this.handler.sendRequest(featureName, request);
    return response;
  }

  getCallbackRouter(): LegionInternalsPageCallbackRouter {
    return this.callbackRouter;
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
