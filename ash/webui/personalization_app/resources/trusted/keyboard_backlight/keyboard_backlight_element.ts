// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import '../../common/styles.js';
import '../cros_button_style.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {isSelectionEvent} from '../../common/utils.js';
import {BacklightColor} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {setBacklightColor} from './keyboard_backlight_controller.js';
import {getTemplate} from './keyboard_backlight_element.html.js';
import {getKeyboardBacklightProvider} from './keyboard_backlight_interface_provider.js';
import {KeyboardBacklightObserver} from './keyboard_backlight_observer.js';


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

interface ColorInfo {
  hexVal: string;
  enumVal: BacklightColor;
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

      rainbowColorId_: {
        type: String,
        value: 'rainbow',
      },

      wallpaperColorId_: {
        type: String,
        value: 'wallpaper',
      },

      /** The color currently highlighted by keyboard navigation. */
      ironSelectedColor_: Object,

      /** The selected backlight color in the system. */
      backlightColor_: Object,
    };
  }

  private presetColors_: Record<string, ColorInfo>;
  private presetColorIds_: string[];
  private rainbowColorId_: string;
  private wallpaperColorId_: string;
  private ironSelectedColor_: HTMLElement;
  private backlightColor_: BacklightColor|null;

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  override connectedCallback() {
    super.connectedCallback();
    KeyboardBacklightObserver.initKeyboardBacklightObserverIfNeeded();
    this.watch<KeyboardBacklight['backlightColor_']>(
        'backlightColor_', state => state.keyboardBacklight.backlightColor);
    this.updateFromStore();
  }

  private computePresetColors_(): Record<string, ColorInfo> {
    return {
      'whiteColor': {hexVal: '#FFFFFF', enumVal: BacklightColor.kWhite},
      'redColor': {hexVal: '#F28B82', enumVal: BacklightColor.kRed},
      'yellowColor': {hexVal: '#FDD663', enumVal: BacklightColor.kYellow},
      'greenColor': {hexVal: '#81C995', enumVal: BacklightColor.kGreen},
      'blueColor': {hexVal: '#78D9EC', enumVal: BacklightColor.kBlue},
      'indigoColor': {hexVal: '#8AB4F8', enumVal: BacklightColor.kIndigo},
      'purpleColor': {hexVal: '#C58AF9', enumVal: BacklightColor.kPurple},
    };
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
      case 'enter':
        switch (this.ironSelectedColor_.id) {
          case this.rainbowColorId_:
            this.onRainbowColorSelected_(e.detail.keyboardEvent);
            break;
          case this.wallpaperColorId_:
            this.onWallpaperColorSelected_(e.detail.keyboardEvent);
            break;
          default:
            // |onPresetColorSelected_| is not invoked here because the event
            // listener target is iron-selector, which results in undefined
            // colorId.
            setBacklightColor(
                this.presetColors_[this.ironSelectedColor_.id].enumVal,
                getKeyboardBacklightProvider(), this.getStore());
            break;
        }
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

  /** Invoked when the wallpaper color is selected. */
  private onWallpaperColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    setBacklightColor(
        BacklightColor.kWallpaper, getKeyboardBacklightProvider(),
        this.getStore());
  }

  /** Invoked when a preset color is selected. */
  private onPresetColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const htmlElement = e.currentTarget as HTMLElement;
    const colorId = htmlElement.id;
    assert(colorId !== undefined, 'colorId not found');
    setBacklightColor(
        this.presetColors_[colorId].enumVal, getKeyboardBacklightProvider(),
        this.getStore());
  }

  /** Invoked when the rainbow color is selected. */
  private onRainbowColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    setBacklightColor(
        BacklightColor.kRainbow, getKeyboardBacklightProvider(),
        this.getStore());
  }

  private getColorInnerContainerStyle_(
      colorId: string, colors: Record<string, ColorInfo>) {
    switch (colorId) {
      case this.rainbowColorId_:
        const hexColors =
            Object.values(colors).map(color => color.hexVal).slice(1);
        return `background-image: linear-gradient(${hexColors})`;
      case this.wallpaperColorId_:
        return `background-color: #8AB4F8`;
      case 'whiteColor':
        // Add the border for the white background.
        return `background-color: ${
            colors[colorId]
                .hexVal}; border: 1px solid var(--cros-separator-color);`;
      default:
        return `background-color: ${colors[colorId].hexVal}`;
    }
  }

  private getPresetColorAriaLabel_(presetColorId: string): string {
    return this.i18n(presetColorId);
  }

  private getWallpaperColorAriaSelected_(selectedColor: BacklightColor) {
    return (selectedColor === BacklightColor.kWallpaper).toString();
  }

  private getPresetColorAriaSelected_(
      colorId: string, colors: Record<string, ColorInfo>,
      selectedColor: BacklightColor) {
    if (!colorId || !colors[colorId]) {
      return 'false';
    }
    return (colors[colorId].enumVal === selectedColor).toString();
  }

  private getRainbowColorAriaSelected(selectedColor: BacklightColor) {
    return (selectedColor === BacklightColor.kRainbow).toString();
  }
}

customElements.define(KeyboardBacklight.is, KeyboardBacklight);
