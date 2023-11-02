// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, GROUP_BUTTON_CLICK} from './events.js';
import {getTemplate} from './text_group_button.html.js';

export class TextGroupButton extends PolymerElement {
  static get is() {
    return 'text-group-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {string} */
      name: {type: String, readonly: true},
      /** @type {string} */
      groupId: {type: String, readonly: true},
      /** @type {boolean} */
      active: {type: Boolean, value: false},
      /** @type {boolean} */
      disabled: {type: Boolean, value: false},
      /** @type {number} */
      customTabIndex: {type: Number, value: -1},
    };
  }

  constructor() {
    super();
  }

  handleClick() {
    this.dispatchEvent(
        createCustomEvent(GROUP_BUTTON_CLICK, {group: this.groupId}));
  }

  _className(active) {
    return active ? 'text-group-active' : '';
  }

  _toUpperCase(text) {
    return text.toUpperCase();
  }
}

customElements.define(TextGroupButton.is, TextGroupButton);
