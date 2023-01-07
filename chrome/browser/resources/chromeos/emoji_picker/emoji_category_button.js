// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_category_button.html.js';
import {CATEGORY_BUTTON_CLICK, createCustomEvent} from './events.js';

export class EmojiCategoryButton extends PolymerElement {
  static get is() {
    return 'emoji-category-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!string} */
      name: {type: String, readonly: true},
      /** @type {!string} */
      icon: {type: String, readonly: true},
      /** @type {!boolean} */
      active: {type: Boolean, value: false},
      /** @type {!boolean} */
      searchActive: {type: Boolean, value: false},
    };
  }

  constructor() {
    super();
  }

  handleClick() {
    this.dispatchEvent(
        createCustomEvent(CATEGORY_BUTTON_CLICK, {categoryName: this.name}));
  }

  _className(active, searchActive) {
    if (searchActive) {
      return 'category-button-primary';
    }
    return active ? 'category-button-active' : '';
  }
}

customElements.define(EmojiCategoryButton.is, EmojiCategoryButton);
