// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './customize_shortcuts.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export interface AppElement {}

export class AppElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-app';
  }

  static get template() {
    return getTemplate();
  }


  static get properties() {
    return {};
  }

  constructor() {
    super();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
