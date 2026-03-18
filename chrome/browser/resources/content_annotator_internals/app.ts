// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';

export class ContentAnnotatorInternalsAppElement extends CrLitElement {
  static get is() {
    return 'content-annotator-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      message_: {type: String},
      logContent_: {type: String},
    };
  }

  protected accessor message_: string = loadTimeData.getString('message');
  protected accessor logContent_: string = 'Loading log content...';
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.loadLogContent_();
  }

  private async loadLogContent_() {
    try {
      const result = await this.browserProxy_.handler.getAnnotatedContent();
      this.logContent_ =
          result.content ?? 'Error retrieving annotated content.';
    } catch (e) {
      this.logContent_ = `Error loading log: ${e}`;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-annotator-internals-app': ContentAnnotatorInternalsAppElement;
  }
}

customElements.define(
    ContentAnnotatorInternalsAppElement.is,
    ContentAnnotatorInternalsAppElement);
