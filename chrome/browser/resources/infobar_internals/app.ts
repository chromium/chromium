// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import type {InfoBarType} from './infobar_internals.mojom-webui.js';


export class InfobarInternalsAppElement extends CrLitElement {
  static get is() {
    return 'infobar-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      infobars: {type: Array},
    };
  }

  protected accessor infobars:
      Array<{type: InfoBarType, name: string, description: string}> = [];

  override connectedCallback() {
    super.connectedCallback();
    BrowserProxy.getInstance().handler.getInfoBars().then(
        ({infobars}) => this.infobars = infobars);
  }

  protected onTrigger(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const typeStr = target.dataset['type'];
    assert(typeStr);

    const typeNum = Number(typeStr);
    assert(!Number.isNaN(typeNum));

    const type = typeNum as InfoBarType;
    this.trigger(type);
  }

  private async trigger(id: InfoBarType) {
    const {success} =
        await BrowserProxy.getInstance().handler.triggerInfoBar(id);
    if (!success) {
      console.warn('Failed to trigger infobar', id);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'infobar-internals-app': InfobarInternalsAppElement;
  }
}

customElements.define(
    InfobarInternalsAppElement.is, InfobarInternalsAppElement);
