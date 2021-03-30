// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, GROUP_BUTTON_CLICK} from './events.js';

export class EmojiGroupButton extends PolymerElement {
  static get is() {
    return 'emoji-group-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!string} */
      name: {type: String, readonly: true},
      /** @type {!string} */
      icon: {type: String, readonly: true},
      /** @type {!string} */
      groupId: {type: String, readonly: true},
      /** @type {!boolean} */
      active: {type: Boolean, value: false},
    };
  }

  constructor() {
    super();
  }

  handleClick(ev) {
    this.dispatchEvent(
        createCustomEvent(GROUP_BUTTON_CLICK, {group: this.groupId}));
  }

  _className(active) {
    return active ? 'emoji-group-active' : '';
  }
}

customElements.define(EmojiGroupButton.is, EmojiGroupButton);
