// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './lacros_app.html.js';

export class LacrosIntroAppElement extends PolymerElement {
  static get is() {
    return 'intro-app';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'intro-app': LacrosIntroAppElement;
  }
}

customElements.define(LacrosIntroAppElement.is, LacrosIntroAppElement);
