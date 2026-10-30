// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './chrome_finds_internals.mojom-webui.js';
import type {BrowserProxy} from './chrome_finds_internals.mojom-webui.js';

export class ChromeFindsInternalsAppElement extends CrLitElement {
  static get is() {
    return 'chrome-finds-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      historyCount_: {type: Number},
      historyJson_: {type: String},
      logs_: {type: Array},
    };
  }

  protected accessor historyCount_: number = 10;
  protected accessor historyJson_: string = '';
  protected accessor logs_: string[] = [];

  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  constructor() {
    super();
    this.browserProxy_.callbackRouter.logMessageAdded.addListener(message => {
      this.appendLog_(message);
    });
  }

  private appendLog_(message: string) {
    this.logs_ = [...this.logs_, message];
    this.updateComplete.then(() => {
      const container = this.shadowRoot.getElementById('log-container');
      if (container) {
        container.scrollTop = container.scrollHeight;
      }
    });
  }

  protected onHistoryCountChange_(e: Event) {
    this.historyCount_ =
        parseInt((e.target as HTMLInputElement).value, 10) || 0;
  }

  protected onRunFindsModelClick_() {
    this.appendLog_('Triggering Finds Service...');
    this.browserProxy_.handler.getFindsServiceModelResponse();
  }

  protected async onDumpHistoryClick_() {
    this.appendLog_(
        `Requesting history JSON (count: ${this.historyCount_})...`);
    const {json} =
        await this.browserProxy_.handler.getHistoryJson(this.historyCount_);
    this.historyJson_ = json;
    this.appendLog_('History JSON received.');
  }

  protected onCopyHistoryClick_() {
    const output = this.shadowRoot.querySelector<HTMLTextAreaElement>(
        '#history-json-output');
    assert(output);

    output.select();
    document.execCommand('copy');
    this.appendLog_('JSON copied to clipboard.');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-finds-internals-app': ChromeFindsInternalsAppElement;
  }
}

customElements.define(
    ChromeFindsInternalsAppElement.is, ChromeFindsInternalsAppElement);
