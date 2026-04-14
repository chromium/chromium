// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import type {SubresourceFilterInternalsPageSettings} from './subresource_filter_internals.mojom-webui.js';

export class SubresourceInternalsAppElement extends CrLitElement {
  static get is() {
    return 'subresource-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shouldHighlightAds_: {type: Boolean},
    };
  }

  protected accessor shouldHighlightAds_: boolean = false;

  private proxy_: BrowserProxy = BrowserProxy.getInstance();
  private listenerId_: number|null = null;

  override async connectedCallback() {
    super.connectedCallback();

    this.listenerId_ =
        this.proxy_.callbackRouter.onInternalsPageSettingsChanged.addListener(
            this.onInternalsPageSettingsChanged_.bind(this));
    this.proxy_.handler.observeInternalsPageSettings(
        this.proxy_.callbackRouter.$.bindNewPipeAndPassRemote());

    const {settings} = await this.proxy_.handler.getInternalsPageSettings();
    this.shouldHighlightAds_ = settings.shouldHighlightAds;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.listenerId_ !== null) {
      this.proxy_.callbackRouter.removeListener(this.listenerId_);
      this.listenerId_ = null;
    }
  }

  protected onHighlightAdsCheckboxChange_(e: Event) {
    const target = e.target as HTMLInputElement;
    this.shouldHighlightAds_ = target.checked;
    this.proxy_.handler.setInternalsPageSettings({
      shouldHighlightAds: this.shouldHighlightAds_,
    });
  }

  private onInternalsPageSettingsChanged_(
      settings: SubresourceFilterInternalsPageSettings) {
    this.shouldHighlightAds_ = settings.shouldHighlightAds;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'subresource-internals-app': SubresourceInternalsAppElement;
  }
}

customElements.define(
    SubresourceInternalsAppElement.is, SubresourceInternalsAppElement);
