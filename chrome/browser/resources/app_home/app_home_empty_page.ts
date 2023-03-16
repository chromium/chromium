// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_home_empty_page.html.js';

export class AppHomeEmptyPageElement extends PolymerElement {
  static get is() {
    return 'app-home-empty-page';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-home-empty-page': AppHomeEmptyPageElement;
  }
}

customElements.define(AppHomeEmptyPageElement.is, AppHomeEmptyPageElement);
