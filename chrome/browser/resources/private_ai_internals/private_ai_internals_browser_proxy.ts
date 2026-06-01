// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrivateAiInternalsPageHandlerRemote, PrivateAiResponseMojoType} from './private_ai_internals.mojom-webui.js';
import {PrivateAiInternalsPageCallbackRouter, PrivateAiInternalsPageHandler} from './private_ai_internals.mojom-webui.js';

/**
 * @fileoverview A browser proxy for the PrivateAi Internals page.
 */

export interface PrivateAiInternalsBrowserProxy {
  connect(
      url: string, apiKey: string, proxyUrl: string,
      useTokenAttestation: boolean): Promise<void>;
  close(): Promise<void>;
  sendRequest(featureName: string, request: string):
      Promise<PrivateAiResponseMojoType>;
  getCallbackRouter(): PrivateAiInternalsPageCallbackRouter;
}

export class PrivateAiInternalsBrowserProxyImpl implements
    PrivateAiInternalsBrowserProxy {
  handler: PrivateAiInternalsPageHandlerRemote;
  callbackRouter: PrivateAiInternalsPageCallbackRouter;

  constructor(handler: PrivateAiInternalsPageHandlerRemote) {
    this.handler = handler;
    this.callbackRouter = new PrivateAiInternalsPageCallbackRouter();

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
      Promise<PrivateAiResponseMojoType> {
    const {response} = await this.handler.sendRequest(featureName, request);
    return response;
  }

  getCallbackRouter(): PrivateAiInternalsPageCallbackRouter {
    return this.callbackRouter;
  }

  static getInstance(): PrivateAiInternalsBrowserProxy {
    return instance ||
        (instance = new PrivateAiInternalsBrowserProxyImpl(
             PrivateAiInternalsPageHandler.getRemote()));
  }

  static setInstance(newInstance: PrivateAiInternalsBrowserProxy) {
    instance = newInstance;
  }
}

let instance: PrivateAiInternalsBrowserProxy|null = null;
