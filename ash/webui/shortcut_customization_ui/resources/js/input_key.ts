// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './input_key.html.js';

const META_KEY = 'meta';

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
export const keyToIconNameMap: {[key: string]: string|undefined} = {
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
  'BrowserSearch': 'browser-search',
  'EmojiPicker': 'emoji-picker',
  'KeyboardBacklightToggle': 'keyboard-brightness-toggle',
  'KeyboardBrightnessUp': 'keyboard-brightness-up',
  'KeyboardBrightnessDown': 'keyboard-brightness-down',
  'LaunchApplication1': 'overview',
  'LaunchApplication2': 'calculator',
  'LaunchAssistant': 'assistant',
  'MediaFastForward': 'fast-forward',
  'MediaPause': 'pause',
  'MediaPlay': 'play',
  'MediaPlayPause': 'play-pause',
  'MediaTrackNext': 'next-track',
  'MediaTrackPrevious': 'last-track',
  'MicrophoneMuteToggle': 'microphone-mute',
  'ModeChange': 'globe',
  'ViewAllApps': 'view-all-apps',
  'Power': 'power',
  'PrintScreen': 'screenshot',
  'PrivacyScreenToggle': 'electronic-privacy-screen',
  'Settings': 'settings',
  'ToggleDictation': 'dictation-toggle',
  'ZoomToggle': 'fullscreen',
};

/**
 * @fileoverview
 * 'input-key' is a component wrapper for a single input key. Responsible for
 * handling dynamic styling of a single key.
 */

const InputKeyElementBase = I18nMixin(PolymerElement);

export class InputKeyElement extends InputKeyElementBase {
  static get is(): string {
    return 'input-key';
  }

  static get properties(): PolymerElementProperties {
    return {
      key: {
        type: String,
        value: '',
        reflectToAttribute: true,
      },

      keyState: {
        type: String,
        value: KeyInputState.NOT_SELECTED,
        reflectToAttribute: true,
      },

      // If this property is true, the spacing between keys will be narrower
      // than usual.
      narrow: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // If this property is true, keys will be styled with the bolder highlight
      // background.
      highlighted: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // This property is used to apply different styling to keys containing
      // only text and those with icons.
      hasIcon: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  key: string;
  keyState: KeyInputState;
  narrow: boolean;
  highlighted: boolean;
  hasIcon: boolean;
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();
    this.hasIcon = this.key in keyToIconNameMap;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private getIconIdForKey(): string|null {
    const hasLauncherButton = this.lookupManager.getHasLauncherButton();
    if (this.key === META_KEY) {
      // 'meta' key should always be the modifier key.
      this.keyState = KeyInputState.MODIFIER_SELECTED;
      return hasLauncherButton ? 'shortcut-customization-keys:launcher' :
                                 'shortcut-customization-keys:search';
    }
    const iconName = keyToIconNameMap[this.key];
    return iconName ? `shortcut-customization-keys:${iconName}` : null;
  }

  /**
   * Returns the GRD string ID for the given key. This function is public and
   * static so that it can be used by the test for this element.
   *
   * @param key The KeyboardEvent.code of a key, e.g. ArrowUp or PrintScreen.
   * @param hasLauncherButton Whether the keyboard has a launcher button or a
   *     search button.
   */
  static getAriaLabelStringId(key: string, hasLauncherButton: boolean): string {
    if (key === META_KEY) {
      return hasLauncherButton ? 'iconLabelOpenLauncher' :
                                 'iconLabelOpenSearch';
    }
    return `iconLabel${key}`;  // e.g. iconLabelArrowUp
  }

  private getAriaLabelForIcon(): string {
    const hasLauncherButton = this.lookupManager.getHasLauncherButton();
    const ariaLabelStringId =
        InputKeyElement.getAriaLabelStringId(this.key, hasLauncherButton);
    assert(
        this.i18nExists(ariaLabelStringId),
        `String ID ${ariaLabelStringId} should exist, but it doesn't.`);

    return this.i18n(ariaLabelStringId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'input-key': InputKeyElement;
  }
}

customElements.define(InputKeyElement.is, InputKeyElement);
