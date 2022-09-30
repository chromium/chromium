// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './touchscreen_tester.html.js';

const TouchscreenTesterElementBase = I18nMixin(PolymerElement);

export class TouchscreenTesterElement extends TouchscreenTesterElementBase {
  static get is() {
    return 'touchscreen-tester';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'touchscreen-tester': TouchscreenTesterElement;
  }
}

customElements.define(TouchscreenTesterElement.is, TouchscreenTesterElement);
