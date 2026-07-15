// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class IwaDevAppElement extends CrLitElement {
  static get is() {
    return 'iwa-dev-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      devModeEnabled: {
        type: Boolean,
      },
    };
  }

  accessor devModeEnabled: boolean =
      loadTimeData.getBoolean('isIwaDevModeEnabled');
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-app': IwaDevAppElement;
  }
}

customElements.define(IwaDevAppElement.is, IwaDevAppElement);
