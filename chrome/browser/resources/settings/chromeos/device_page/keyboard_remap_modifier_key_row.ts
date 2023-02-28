// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'keyboard-remap-key-row' contains a key with icon label and dropdown menu to
 * allow users to customize the remapped key.
 */

import '../../settings_shared.css.js';
import '../../controls/settings_dropdown_menu.js';
import '../../prefs/prefs.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../../controls/settings_dropdown_menu.js';

import {MetaKey, ModifierKey} from './input_device_settings_types.js';
import {getTemplate} from './keyboard_remap_modifier_key_row.html.js';

/**
 * Refers to the state of an 'remap-key' icon.
 */
enum KeyState {
  DEFAULT_REMAPPING = 'default-remapping',
  MODIFIER_REMAPPED = 'modifier-remapped',
}

/**
 * Mapping for each modifier key to its default remapping key.
 */
let defaultRemappings: {[key: number]: ModifierKey} = {
  [ModifierKey.META]: ModifierKey.META,
  [ModifierKey.CONTROL]: ModifierKey.CONTROL,
  [ModifierKey.ALT]: ModifierKey.ALT,
  [ModifierKey.ESC]: ModifierKey.ESC,
  [ModifierKey.BACKSPACE]: ModifierKey.BACKSPACE,
  [ModifierKey.ASSISTANT]: ModifierKey.ASSISTANT,
  [ModifierKey.CAPS_LOCK]: ModifierKey.CAPS_LOCK,
};

export class KeyboardRemapModifierKeyRowElement extends PolymerElement {
  static get is(): string {
    return 'keyboard-remap-modifier-key-row';
  }

  static get properties(): PolymerElementProperties {
    return {
      keyLabel: {
        type: String,
        value: '',
      },

      keyState: {
        type: String,
        value: KeyState.DEFAULT_REMAPPING,
        reflectToAttribute: true,
        computed: 'computeKeyState(pref.value)',
      },

      menuOptions: {
        type: Array,
      },

      pref: {
        type: Object,
      },

      metaKey: {
        type: Number,
        observer: 'updateDefaultRemapping',
      },

      key: {
        type: Number,
      },
    };
  }

  keyLabel: string;
  keyState: KeyState;
  menuOptions: DropdownMenuOptionList;
  pref: chrome.settingsPrivate.PrefObject;
  metaKey: MetaKey;
  key: ModifierKey;

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  /**
   * Whenever the key remapping is changed, update the keyState to change
   * the icon color between default and highlighted.
   */
  private computeKeyState(): KeyState {
    return defaultRemappings[this.key] === this.pref.value ?
        KeyState.DEFAULT_REMAPPING :
        KeyState.MODIFIER_REMAPPED;
  }

  private updateDefaultRemapping(): void {
    defaultRemappings = {
      ...defaultRemappings,
      [ModifierKey.META]:
          this.metaKey === MetaKey.COMMAND ? ModifierKey.CONTROL :
                                             ModifierKey.META,
      [ModifierKey.CONTROL]:
          this.metaKey === MetaKey.COMMAND ? ModifierKey.META :
                                             ModifierKey.CONTROL,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'keyboard-remap-modifier-key-row': KeyboardRemapModifierKeyRowElement;
  }
}

customElements.define(
    KeyboardRemapModifierKeyRowElement.is, KeyboardRemapModifierKeyRowElement);