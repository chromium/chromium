// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './chrome_finds_internals.mojom-webui.js';
import {DEFAULT_PROMPT} from './constants.js';

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
      prompt_: {type: String},
      historyJson_: {type: String},
      logs_: {type: Array},
    };
  }

  protected accessor historyCount_: number = 10;
  protected accessor prompt_: string = DEFAULT_PROMPT;
  protected accessor historyJson_: string = '';
  protected accessor logs_: string[] = [];

  private handler_: PageHandlerRemote = new PageHandlerRemote();
  private callbackRouter_: PageCallbackRouter = new PageCallbackRouter();

  constructor() {
    super();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());

    this.callbackRouter_.logMessageAdded.addListener((message: string) => {
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

  protected onPromptInput_(e: Event) {
    this.prompt_ = (e.target as HTMLTextAreaElement).value;
  }

  protected onStartClick_() {
    this.handler_.start(this.prompt_, this.historyCount_);
  }

  protected onResetClick_() {
    this.prompt_ = DEFAULT_PROMPT;
    this.appendLog_('Prompt reset to default.');
  }

  protected async onDumpHistoryClick_() {
    this.appendLog_(
        `Requesting history JSON (count: ${this.historyCount_})...`);
    const {json} = await this.handler_.getHistoryJson(this.historyCount_);
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
