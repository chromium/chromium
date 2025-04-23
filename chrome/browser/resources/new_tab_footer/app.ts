// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class NewTabFooterAppElement extends CrLitElement {
  static get is() {
    return 'new-tab-footer-app';
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

  // TODO(crbug.com/409056431): Remove `message_` once relevant
  // variables/properties are added. This is used as a placeholder.
  protected accessor message_: string = loadTimeData.getString('message');
}

declare global {
  interface HTMLElementTagNameMap {
    'new-tab-footer-app': NewTabFooterAppElement;
  }
}

customElements.define(NewTabFooterAppElement.is, NewTabFooterAppElement);
