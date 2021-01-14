// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GROUP_BUTTON_EVENT, GroupButtonEvent} from './events.js';

class EmojiGroupButton extends PolymerElement {
  static get is() {
    return 'emoji-group-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {string} */
      icon: {type: String},
      group: {type: String},
      active: {type: Boolean, value: false},
    };
  }

  constructor() {
    super();
  }

  handleClick(ev) {
    /** @type {GroupButtonEvent} */
    const event = new CustomEvent(
        GROUP_BUTTON_EVENT,
        {bubbles: true, composed: true, detail: {group: this.group}});
    this.dispatchEvent(event);
  }

  _className(active) {
    return active ? 'active' : '';
  }
}

customElements.define(EmojiGroupButton.is, EmojiGroupButton);
