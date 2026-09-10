// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {enumToClient} from '../../enum_conversions.js';
import {GeminiEnterpriseHandlerRemote} from '../../gemini_enterprise.mojom-webui.js';
import type {WebClientHandlerRemote, WebClientInitialState} from '../../glic.mojom-webui.js';
import type {CloseSignInTabOptions, CloseSignInTabResult, GeicBrowserHost, OpenSignInTabOptions, OpenSignInTabResult} from '../../glic_api/glic_api.js';

export class GlicBrowserHostGeic implements GeicBrowserHost {
  private handler?: WebClientHandlerRemote;
  private handlerRemote?: GeminiEnterpriseHandlerRemote;

  initialize(
      _initialState: WebClientInitialState, handler?: WebClientHandlerRemote) {
    this.handler = handler;
  }

  destroy(): void {
    if (this.handlerRemote) {
      this.handlerRemote.$.close();
      this.handlerRemote = undefined;
    }
    this.handler = undefined;
  }

  async openSignInTab(options?: OpenSignInTabOptions):
      Promise<OpenSignInTabResult> {
    const mojoOptions = options ? {
      signinUrl: options.signinUrl ?? null,
    } :
                                  null;
    const response = await this.getGeicHandler().openSignInTab(mojoOptions);
    return enumToClient(response.result);
  }

  async closeSignInTab(options?: CloseSignInTabOptions):
      Promise<CloseSignInTabResult> {
    const response =
        await this.getGeicHandler().closeSignInTab(options ?? null);
    return enumToClient(response.result);
  }

  private getGeicHandler(): GeminiEnterpriseHandlerRemote {
    assert(this.handler);
    if (!this.handlerRemote) {
      this.handlerRemote = new GeminiEnterpriseHandlerRemote();
      this.handler.createGeminiEnterpriseHandler(
          this.handlerRemote.$.bindNewPipeAndPassReceiver());
    }
    return this.handlerRemote;
  }
}
