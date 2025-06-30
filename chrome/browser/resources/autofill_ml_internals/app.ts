// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

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
      message_: {type: String},
    };
  }

  protected accessor message_: string = loadTimeData.getString('message');
}

declare global {
  interface HTMLElementTagNameMap {
    'autofill-ml-internals-app': AutofillMlInternalsAppElement;
  }
}

customElements.define(
    AutofillMlInternalsAppElement.is, AutofillMlInternalsAppElement);
