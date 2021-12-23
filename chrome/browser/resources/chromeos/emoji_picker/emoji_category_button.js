// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class EmojiCategoryButton extends PolymerElement {
  static get is() {
    return 'emoji-category-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!string} */
      icon: {type: String, readonly: true}
    };
  }

  constructor() {
    super();
  }
}

customElements.define(EmojiCategoryButton.is, EmojiCategoryButton);
