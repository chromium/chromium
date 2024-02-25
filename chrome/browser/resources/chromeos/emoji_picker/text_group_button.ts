// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

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
      name: {type: String, readonly: true},
      groupId: {type: String, readonly: true},
      active: {type: Boolean, value: false},
      disabled: {type: Boolean, value: false},
      customTabIndex: {type: Number, value: -1},
    };
  }
  name: string;
  groupId: string;
  active: boolean;
  disabled: boolean;
  customTabIndex: number;

  constructor() {
    super();
  }

  handleClick() {
    this.dispatchEvent(
        createCustomEvent(GROUP_BUTTON_CLICK, {group: this.groupId}));
  }

  private calculateClassName(active: boolean) {
    return active ? 'text-group-active' : '';
  }

  private getAriaPressedState(active: boolean): string {
    return active ? 'true' : 'false';
  }
}

customElements.define(TextGroupButton.is, TextGroupButton);
