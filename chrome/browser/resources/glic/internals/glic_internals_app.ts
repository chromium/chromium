// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import type {ProfileEnablement} from '../glic.mojom-webui.js';

import {getCss} from './glic_internals_app.css.js';
import {getHtml} from './glic_internals_app.html.js';


export class GlicInternalsAppElement extends CrLitElement {
  static get is() {
    return 'glic-internals-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      enablement_: {type: Object},
    };
  }

  protected accessor enablement_: ProfileEnablement|undefined;

  private browserProxy_ = new BrowserProxyImpl();

  override connectedCallback() {
    super.connectedCallback();
    this.browserProxy_.pageHandler.getProfileEnablement().then(
        ({enablement}) => {
          this.enablement_ = enablement;
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-internals-app': GlicInternalsAppElement;
  }
}

customElements.define(GlicInternalsAppElement.is, GlicInternalsAppElement);
