// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './event_list/event_list.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';

export class UpdaterAppElement extends CrLitElement {
  static get is() {
    return 'updater-app';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      messages: {type: Array},
    };
  }

  protected accessor messages: Array<Record<string, unknown>> = [];

  override connectedCallback() {
    super.connectedCallback();
    this.getAllUpdaterEvents().then(messages => this.messages = messages);
  }

  override render() {
    return getHtml.bind(this)();
  }

  private async getAllUpdaterEvents(): Promise<Array<Record<string, unknown>>> {
    const response =
        await BrowserProxyImpl.getInstance().handler.getAllUpdaterEvents();

    return response.events.map(message => JSON.parse(message))
        .filter(message => typeof message === 'object');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'updater-app': UpdaterAppElement;
  }
}

customElements.define(UpdaterAppElement.is, UpdaterAppElement);
