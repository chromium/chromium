// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';

export class AccessibilityAnnotatorInternalsAppElement extends CrLitElement {
  static get is() {
    return 'accessibility-annotator-internals-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      message_: {type: String},
    };
  }

  protected accessor message_: string = '';
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected async onTriggerFirstRunClick_() {
    this.message_ = '';
    try {
      const {success} = await this.browserProxy_.handler.triggerFirstRun();
      if (success) {
        this.message_ = 'Trigger First Run succeeded.';
      } else {
        this.message_ = 'Trigger First Run failed or was ignored.';
      }
    } catch (e) {
      this.message_ = 'Error: could not trigger first run.';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accessibility-annotator-internals-app':
        AccessibilityAnnotatorInternalsAppElement;
  }
}

customElements.define(
    AccessibilityAnnotatorInternalsAppElement.is,
    AccessibilityAnnotatorInternalsAppElement);
