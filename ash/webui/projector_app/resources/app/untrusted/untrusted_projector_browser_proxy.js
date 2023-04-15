// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {UntrustedProjectorPageCallbackRouter, UntrustedProjectorPageHandlerFactory, UntrustedProjectorPageHandlerRemote, UntrustedProjectorPageRemote} from './ash/webui/projector_app/mojom/untrusted_projector.mojom-webui.js';


export class UntrustedProjectorBrowserProxyImpl {
  constructor() {
    this.pageHandlerFactory = UntrustedProjectorPageHandlerFactory.getRemote();
    this.pageHandlerRemote = new UntrustedProjectorPageHandlerRemote();
    this.projectorCallbackRouter = new UntrustedProjectorPageCallbackRouter();
    this.pageHandlerFactory.create(
        this.pageHandlerRemote.$.bindNewPipeAndPassReceiver(),
        this.projectorCallbackRouter.$.bindNewPipeAndPassRemote());
  }

  getProjectorCallbackRouter() {
    return this.projectorCallbackRouter;
  }

  async getNewScreencastPreconditionState() {
    const {precondition} =
        await this.pageHandlerRemote.getNewScreencastPrecondition();
    return precondition;
  }

  async shouldDownloadSoda() {
    const {shouldDownload} = await this.pageHandlerRemote.shouldDownloadSoda();
    return shouldDownload;
  }

  async installSoda() {
    const {triggered} = await this.pageHandlerRemote.installSoda();
    return triggered;
  }
}

/**
 * @type {UntrustedProjectorBrowserProxyImpl}
 */
export const browserProxy = new UntrustedProjectorBrowserProxyImpl();
