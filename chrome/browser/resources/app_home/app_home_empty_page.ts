// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app_home_empty_page.css.js';
import {getHtml} from './app_home_empty_page.html.js';

export class AppHomeEmptyPageElement extends CrLitElement {
  static get is() {
    return 'app-home-empty-page';
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
    'app-home-empty-page': AppHomeEmptyPageElement;
  }
}

customElements.define(AppHomeEmptyPageElement.is, AppHomeEmptyPageElement);
