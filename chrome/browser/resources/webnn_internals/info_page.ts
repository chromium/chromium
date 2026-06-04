// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_win">
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

// </if>

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './info_page.css.js';
import {getHtml} from './info_page.html.js';
import {browserProxyFactory} from './webnn_internals.mojom-webui.js';
import type {BrowserProxy} from './webnn_internals.mojom-webui.js';
import type {WebNNExecutionProviderDetails} from './webnn_service_introspection.mojom-webui.js';

export class WebnnInternalsInfoPageElement extends CrLitElement {
  static get is() {
    return 'webnn-internals-info-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      availableExecutionProviders_: {type: Array},
    };
  }

  protected accessor availableExecutionProviders_:
      WebNNExecutionProviderDetails[] = [];
  private listenerIds_: number[] = [];

  private proxy_: BrowserProxy = browserProxyFactory.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.handler.requestAvailableExecutionProvidersDetails().then(
        ({availableExecutionProviders}) => {
          this.onUpdateAvailableExecutionProvidersDetails_(
              availableExecutionProviders);
        });
    this.listenerIds_ =
        [this.proxy_.callbackRouter.onUpdateAvailableExecutionProvidersDetails
             .addListener(
                 this.onUpdateAvailableExecutionProvidersDetails_.bind(this))];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.proxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  // <if expr="is_win">
  protected async onForceOrtEnvCreationClick_() {
    this.onUpdateAvailableExecutionProvidersDetails_(
        (await this.proxy_.handler
             .forceOrtEnvironmentCreationForIntrospection())
            .availableExecutionProviders);
  }
  // </if>

  private onUpdateAvailableExecutionProvidersDetails_(
      availableExecutionProviders: WebNNExecutionProviderDetails[]) {
    this.availableExecutionProviders_ = availableExecutionProviders;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webnn-internals-info-page': WebnnInternalsInfoPageElement;
  }
}

customElements.define(
    WebnnInternalsInfoPageElement.is, WebnnInternalsInfoPageElement);
