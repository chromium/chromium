// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import '../../common/styles.js';
import '../cros_button_style.js';

import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './keyboard_backlight_element.html.js';


/**
 * @fileoverview
 * The keyboard backlight section that allows users to customize their keyboard
 * backlight colors.
 */

export interface KeyboardBacklight {
  $: {
    keys: IronA11yKeysElement,
    selector: IronSelectorElement,
  };
}

export class KeyboardBacklight extends WithPersonalizationStore {
  static get is() {
    return 'keyboard-backlight';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      presetColors_: {
        type: Object,
        computed: 'computePresetColors_()',
      },

      presetColorIds_: {
        type: Array,
        computed: 'computePresetColorIds_(presetColors_)',
      },

      /** The color currently highlighted by keyboard navigation. */
      ironSelectedColor_: Object,
    };
  }

  private presetColors_: Record<string, string>;
  private presetColorIds_: string[];
  private ironSelectedColor_: HTMLElement;

  private computePresetColors_(): Record<string, string> {
    return {
      'white': '#FFFFFF',
      'red': '#F28B82',
      'yellow': '#FDD663',
      'green': '#81C995',
      'blue': '#78D9EC',
      'indigo': '#8AB4F8',
      'purple': '#C58AF9',
      'rainbow': '',
    };
  }

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  private computePresetColorIds_(presetColors: Record<string, string>):
      string[] {
    // ES2020 maintains ordering of Object.keys.
    return Object.keys(presetColors);
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.selector;
    const prevButton = this.ironSelectedColor_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous button.
    if (prevButton) {
      prevButton.removeAttribute('tabindex');
    }
    // Add focus state for new button.
    if (this.ironSelectedColor_) {
      this.ironSelectedColor_.setAttribute('tabindex', '0');
      this.ironSelectedColor_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  private getColorInnerContainerStyle_(
      color: string, colors: Record<string, string>) {
    if (color === 'rainbow') {
      // ES2020 maintain the ordering of Object.values.
      return `background-image: linear-gradient(${
          Object.values(colors).slice(1, -1)})`;
    }
    // Add the border for the white background.
    if (color === 'white') {
      return `background-color: ${
          colors[color]}; border: 1px solid var(--cros-separator-color);`;
    }
    return `background-color: ${colors[color]}`;
  }
}

customElements.define(KeyboardBacklight.is, KeyboardBacklight);
