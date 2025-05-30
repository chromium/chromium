// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage certificates" section
 * to interact with the browser. For the V2 Certificate Manager only.
 */

import type {CertificateManagerPageHandlerInterface} from './certificate_manager.mojom-webui.js';
import {CertificateManagerPageCallbackRouter, CertificateManagerPageHandlerFactory, CertificateManagerPageHandlerRemote} from './certificate_manager.mojom-webui.js';

export class CertificatesBrowserProxy {
  callbackRouter: CertificateManagerPageCallbackRouter;
  handler: CertificateManagerPageHandlerInterface;

  constructor() {
    this.callbackRouter = new CertificateManagerPageCallbackRouter();

    this.handler = new CertificateManagerPageHandlerRemote();

    const factory = CertificateManagerPageHandlerFactory.getRemote();
    factory.createCertificateManagerPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as CertificateManagerPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): CertificatesBrowserProxy {
    return instance || (instance = new CertificatesBrowserProxy());
  }

  static setInstance(obj: CertificatesBrowserProxy) {
    instance = obj;
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
let instance: CertificatesBrowserProxy|null = null;
