// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './log_list.js';
import './log_details.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {MlPredictionLog} from './autofill_ml_internals.mojom-webui.js';
import {AutofillMlInternalsBrowserProxy} from './browser_proxy.js';


export class AppElement extends CrLitElement {
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
      logEntries_: {type: Array},
      selectedLog_: {type: Object},
    };
  }

  protected accessor logEntries_: MlPredictionLog[] = [];
  protected accessor selectedLog_: MlPredictionLog|null = null;

  private browserProxy_: AutofillMlInternalsBrowserProxy =
      AutofillMlInternalsBrowserProxy.getInstance();
  private onLogAddedListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.browserProxy_.handler.setPage(
        this.browserProxy_.callbackRouter.$.bindNewPipeAndPassRemote());
    this.onLogAddedListenerId_ =
        this.browserProxy_.callbackRouter.onLogAdded.addListener(
            (log: MlPredictionLog) => {
              this.logEntries_ = [log, ...this.logEntries_];
              this.selectedLog_ = log;
            });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.onLogAddedListenerId_ !== null) {
      this.browserProxy_.callbackRouter.removeListener(
          this.onLogAddedListenerId_);
      this.onLogAddedListenerId_ = null;
    }
  }

  protected onLogSelected_(e: CustomEvent<MlPredictionLog>) {
    this.selectedLog_ = e.detail;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'autofill-ml-internals-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
