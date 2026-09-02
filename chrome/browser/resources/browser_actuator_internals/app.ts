// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class BrowserActuatorInternalsAppElement extends CrLitElement {
  static get is() {
    return 'browser-actuator-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'browser-actuator-internals-app': BrowserActuatorInternalsAppElement;
  }
}

customElements.define(
    BrowserActuatorInternalsAppElement.is, BrowserActuatorInternalsAppElement);
