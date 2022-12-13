// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './input_key.html.js';

/**
 * Refers to the state of an 'input-key' item.
 */
export enum KeyInputState {
  NOT_SELECTED = 'not-selected',
  MODIFIER_SELECTED = 'modifier-selected',
  ALPHANUMERIC_SELECTED = 'alpha-numeric-selected',
}

// The keys in this map are pulled from the file:
// ui/events/keycodes/dom/dom_code_data.inc
// TODO(cambickel): Add remaining missing icons.
export const keyToIconNameMap: {[key: string]: string} = {
  'ArrowDown': 'arrow-down',
  'ArrowLeft': 'arrow-left',
  'ArrowRight': 'arrow-right',
  'ArrowUp': 'arrow-up',
  'AudioVolumeDown': 'volume-down',
  'AudioVolumeMute': 'volume-mute',
  'AudioVolumeUp': 'volume-up',
  'BrightnessDown': 'display-brightness-down',
  'BrightnessUp': 'display-brightness-up',
  'BrowserBack': 'back',
  'BrowserForward': 'forward',
  'BrowserRefresh': 'refresh',
  'KeyboardBacklightToggle': 'keyboard-brightness-toggle',
  'KeyboardBrightnessUp': 'keyboard-brightness-up',
  'KeyboardBrightnessDown': 'keyboard-brightness-down',
  'LaunchApplication1': 'overview',
  'LaunchAssistant': 'assistant',
  'MediaPlayPause': 'play-pause',
  'MediaTrackNext': 'next-track',
  'MediaTrackPrevious': 'last-track',
  'MicrophoneMuteToggle': 'microphone-mute',
  // TODO(cambickel) The launcher icon will vary per-device; update this when
  // we're able to detect which one to show.
  'OpenLauncher': 'launcher',
  'Power': 'power',
  'PrintScreen': 'screenshot',
  'PrivacyScreenToggle': 'electronic-privacy-screen',
  'ZoomToggle': 'fullscreen',
};

/**
 * @fileoverview
 * 'input-key' is a component wrapper for a single input key. Responsible for
 * handling dynamic styling of a single key.
 */
export class InputKeyElement extends PolymerElement {
  static get is() {
    return 'input-key';
  }

  static get properties() {
    return {
      key: {
        type: String,
        value: '',
      },

      keyState: {
        type: String,
        value: KeyInputState.NOT_SELECTED,
        reflectToAttribute: true,
      },
    };
  }

  key: string;
  keyState: KeyInputState;

  static get template() {
    return getTemplate();
  }

  private getIconIdForKey(): string|null {
    const iconName = keyToIconNameMap[this.key];
    if (iconName) {
      return `shortcut-customization-keys:${iconName}`;
    }
    return null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'input-key': InputKeyElement;
  }
}

customElements.define(InputKeyElement.is, InputKeyElement);
