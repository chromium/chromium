// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {PageCallbackRouter, PageHandler} from './autofill_ml_internals.mojom-webui.js';
import type {MLPredictionLog, PageHandlerRemote} from './autofill_ml_internals.mojom-webui.js';


export class AutofillMlInternalsAppElement extends CrLitElement {
  static get is() {
    return 'autofill-ml-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      logs_: {type: Array},
    };
  }

  protected accessor logs_: MLPredictionLog[] = [];

  private pageHandler_: PageHandlerRemote;
  private callbackRouter_: PageCallbackRouter;

  constructor() {
    super();

    this.pageHandler_ = PageHandler.getRemote();
    this.callbackRouter_ = new PageCallbackRouter();

    this.pageHandler_.setPage(
        this.callbackRouter_.$.bindNewPipeAndPassRemote());

    this.callbackRouter_.onLogAdded.addListener(
        (log: MLPredictionLog) => this.onLogAdded_(log));
  }

  private onLogAdded_(log: MLPredictionLog) {
    this.logs_ = [log, ...this.logs_];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'autofill-ml-internals-app': AutofillMlInternalsAppElement;
  }
}

customElements.define(
    AutofillMlInternalsAppElement.is, AutofillMlInternalsAppElement);
