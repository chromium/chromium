// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PageHandlerFactory, PageHandlerRemote} from './ai_overlay_dialog.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class AppElement extends CrLitElement {
  static get is() {
    return 'ai-overlay-dialog-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isListening: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected accessor isListening: boolean = false;

  private pageHandler: PageHandlerRemote;

  // API key to use to connect to backend. Only used for development when
  // provided on command line.
  private apiKey: string = '';

  // If onStateClick_ happens before the API key mojo returns, this will turn
  // to true and invoke the state change after the key becomes available.
  private queueStateChange: boolean = false;

  constructor() {
    super();

    // Setup Mojo connection
    this.pageHandler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.pageHandler.$.bindNewPipeAndPassReceiver());

    this.pageHandler.getApiKey().then(({apiKey}) => {
      this.apiKey = apiKey;
      if (this.queueStateChange) {
        this.onStateClick_();
        this.queueStateChange = false;
      }
    });
  }

  protected onStateClick_() {
    if (!this.apiKey) {
      console.warn('API key not yet available');
      this.queueStateChange = true;
      return;
    }
    this.isListening = !this.isListening;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-overlay-dialog-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
