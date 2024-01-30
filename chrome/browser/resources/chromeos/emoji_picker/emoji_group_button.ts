// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_group_button.html.js';
import {createCustomEvent, GROUP_BUTTON_CLICK, GroupButtonClickEvent} from './events.js';

export class EmojiGroupButton extends PolymerElement {
  static get is() {
    return 'emoji-group-button' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      active: {type: Boolean, value: false},
      customTabIndex: {type: Number, value: -1},
      disabled: {type: Boolean, value: false},
      groupId: {type: String, readonly: true},
      icon: {type: String, readonly: true},
      name: {type: String, readonly: true},
    };
  }

  active: boolean;
  customTabIndex: number;
  disabled: boolean;
  groupId: string;
  icon: string;
  name: string;

  handleClick(): void {
    this.dispatchEvent(
        createCustomEvent(GROUP_BUTTON_CLICK, {group: this.groupId}));
  }

  private calculateClassName(active: boolean): string {
    return active ? 'emoji-group-active' : '';
  }

  private getAriaPressedState(active: boolean): string {
    return active ? 'true' : 'false';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiGroupButton.is]: EmojiGroupButton;
  }
}

declare global {
  interface HTMLElementEventMap {
    [GROUP_BUTTON_CLICK]: GroupButtonClickEvent;
  }
}

customElements.define(EmojiGroupButton.is, EmojiGroupButton);
