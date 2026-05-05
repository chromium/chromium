// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './contexts_viewer.css.js';
import {getHtml} from './contexts_viewer.html.js';
import type {WebNNContextIntrospectionDetails, WebNNExecutionProviderDetails} from './webnn_service_introspection.mojom-webui.js';

export class WebnnInternalsContextsViewerElement extends CrLitElement {
  static get is() {
    return 'webnn-internals-contexts-viewer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      contexts_: {type: Array},
    };
  }

  private listenerIds_: number[] = [];
  protected accessor contexts_: Array<{
    contextId: string,
    contextBackend: string,
    executionProviders: WebNNExecutionProviderDetails[],
  }> = [];

  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds_ =
        [this.proxy_.callbackRouter.onUpdateExistingContextDetails.addListener(
            this.onUpdateExistingContexts_.bind(this))];
    this.proxy_.handler.requestExistingContextsDetails().then(
        ({contextsInfo}:
             {contextsInfo: WebNNContextIntrospectionDetails[]}) => {
          this.onUpdateExistingContexts_(contextsInfo);
        });
  }

  private onUpdateExistingContexts_(
      contexts: WebNNContextIntrospectionDetails[]) {
    this.contexts_ =
        contexts.map(context => ({
                       contextId: context.contextId.toString(),
                       contextBackend: context.contextBackend,
                       executionProviders: context.executionProviders,
                     }));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.proxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webnn-internals-contexts-viewer': WebnnInternalsContextsViewerElement;
  }
}

customElements.define(
    WebnnInternalsContextsViewerElement.is,
    WebnnInternalsContextsViewerElement);
