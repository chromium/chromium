// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'keyboard-remap-key-row' contains a key with icon label and dropdown menu to
 * allow users to customize the remapped key.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import '../controls/settings_dropdown_menu.js';
import '../os_settings_icons.html.js';
// <if expr="_google_chrome" >
import 'chrome://resources/ash/common/internal/ash_internal_icons.html.js';
// </if>

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';

import {MetaKey, ModifierKey} from './input_device_settings_types.js';
import {getTemplate} from './keyboard_remap_modifier_key_row.html.js';

/**
 * Refers to the state of an 'remap-key' icon.
 */
enum KeyState {
  DEFAULT_REMAPPING = 'default-remapping',
  MODIFIER_REMAPPED = 'modifier-remapped',
}

type KeyIcon = 'cr:search'|'os-settings:launcher'|'os-settings:assistant'|
    'ash-internal:launcher-refresh'|'ash-internal:right-alt'|'';
const KeyboardRemapModifierKeyRowElementBase = I18nMixin(PolymerElement);

export class KeyboardRemapModifierKeyRowElement extends
    KeyboardRemapModifierKeyRowElementBase {
  static get is() {
    return 'keyboard-remap-modifier-key-row';
  }

  static get properties(): PolymerElementProperties {
    return {
      keyLabel: {
        type: String,
        value: '',
        computed: 'getKeyLabel(metaKey)',
      },

      metaKeyLabel: {
        type: String,
        value: '',
        computed: 'getMetaKeyLabel(metaKey)',
      },

      keyState: {
        type: String,
        value: KeyState.DEFAULT_REMAPPING,
        reflectToAttribute: true,
        computed: 'computeKeyState(pref.value, defaultRemappings.*)',
      },

      pref: {
        type: Object,
      },

      metaKey: {
        type: Number,
      },

      key: {
        type: Number,
      },

      defaultRemappings: {
        type: Object,
      },

      keyMapTargets: {
        type: Object,
      },

      keyIcon: {
        type: String,
        value: '',
        computed: 'getKeyIcon(key, metaKey)',
      },

      removeTopBorder: {
        type: Boolean,
        reflectToAttribute: true,
      },

      hasFunctionKey: {
        type: Boolean,
        value: false,
      },
    };
  }

  protected keyLabel: string;
  private metaKeyLabel: string;
  private keyMapTargets: DropdownMenuOptionList;
  private keyIcon: KeyIcon;
  private hasFunctionKey: boolean;
  keyState: KeyState;
  pref: chrome.settingsPrivate.PrefObject;
  metaKey: MetaKey;
  key: ModifierKey;
  defaultRemappings: {[key: number]: ModifierKey};

  override ready(): void {
    super.ready();

    this.setUpKeyMapTargets();
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  /**
   * Whenever the key remapping is changed, update the keyState to change
   * the icon color between default and highlighted.
   */
  private computeKeyState(): KeyState {
    return this.defaultRemappings[this.key] === this.pref.value ?
        KeyState.DEFAULT_REMAPPING :
        KeyState.MODIFIER_REMAPPED;
  }

  /**
   * Populate the metaKey label according to metaKey.
   */
  private getMetaKeyLabel(): string {
    switch (this.metaKey) {
      case MetaKey.kCommand: {
        return this.i18n('perDeviceKeyboardKeyCommand');
      }
      case MetaKey.kExternalMeta: {
        return this.i18n('perDeviceKeyboardKeyMeta');
      }
      // Launcher and Search key will display icon instead of text.
      case MetaKey.kLauncher:
      case MetaKey.kSearch:
      case MetaKey.kLauncherRefresh:
        return this.i18n('perDeviceKeyboardKeySearch');
    }
  }

  /**
   * Populate the key label inside the keyboard key icon.
   */
  private getKeyLabel(): string {
    switch (this.key) {
      case ModifierKey.kAlt: {
        return this.i18n('perDeviceKeyboardKeyAlt');
      }
      case ModifierKey.kAssistant: {
        return this.i18n('perDeviceKeyboardKeyAssistant');
      }
      case ModifierKey.kBackspace: {
        return this.i18n('perDeviceKeyboardKeyBackspace');
      }
      case ModifierKey.kCapsLock: {
        return this.i18n('perDeviceKeyboardKeyCapsLock');
      }
      case ModifierKey.kControl: {
        return this.i18n('perDeviceKeyboardKeyCtrl');
      }
      case ModifierKey.kEscape: {
        return this.i18n('perDeviceKeyboardKeyEscape');
      }
      case ModifierKey.kMeta: {
        return this.getMetaKeyLabel();
      }
      case ModifierKey.kRightAlt: {
        return this.i18n('perDeviceKeyboardKeyRightAlt');
      }
      case ModifierKey.kFunction: {
        return this.i18n('perDeviceKeyboardKeyFunction');
      }
      default:
        assertNotReached('Invalid modifier key: ' + this.key);
    }
  }

  private setUpKeyMapTargets(): void {
    // Ordering is according to UX, but values match ModifierKey.
    this.keyMapTargets = (() => {
      const keyMapTargets = [
        {
          value: ModifierKey.kMeta,
          name: this.i18n('perDeviceKeyboardKeySearch'),
        },
        {
          value: ModifierKey.kControl,
          name: this.i18n('perDeviceKeyboardKeyCtrl'),
        },
        {
          value: ModifierKey.kAlt,
          name: this.i18n('perDeviceKeyboardKeyAlt'),
        },
        {
          value: ModifierKey.kCapsLock,
          name: this.i18n('perDeviceKeyboardKeyCapsLock'),
        },
        {
          value: ModifierKey.kEscape,
          name: this.i18n('perDeviceKeyboardKeyEscape'),
        },
        {
          value: ModifierKey.kBackspace,
          name: this.i18n('perDeviceKeyboardKeyBackspace'),
        },
        {
          value: ModifierKey.kAssistant,
          name: this.i18n('perDeviceKeyboardKeyAssistant'),
        },
      ];

      if (loadTimeData.getBoolean('enableModifierSplit')) {
        keyMapTargets.push({
          value: ModifierKey.kRightAlt,
          name: this.i18n('perDeviceKeyboardKeyRightAlt'),
        });
      }

      if (this.hasFunctionKey) {
        keyMapTargets.push({
          value: ModifierKey.kFunction,
          name: this.i18n('perDeviceKeyboardKeyFunction'),
        });
      }

      // Push void last so that right alt is added before it.
      keyMapTargets.push({
        value: ModifierKey.kVoid,
        name: this.i18n('perDeviceKeyboardKeyDisabled'),
      });

      return keyMapTargets;
    })();
  }

  private getKeyIcon(): KeyIcon {
    if (this.key === ModifierKey.kMeta) {
      if (this.metaKey === MetaKey.kSearch) {
        return 'cr:search';
      }
      if (this.metaKey === MetaKey.kLauncher) {
        return 'os-settings:launcher';
      }
      if (this.metaKey === MetaKey.kLauncherRefresh) {
        return 'ash-internal:launcher-refresh';
      }
    } else if (this.key === ModifierKey.kAssistant) {
      return 'os-settings:assistant';
    } else if (this.key === ModifierKey.kRightAlt) {
      return 'ash-internal:right-alt';
    }

    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'keyboard-remap-modifier-key-row': KeyboardRemapModifierKeyRowElement;
  }
}

customElements.define(
    KeyboardRemapModifierKeyRowElement.is, KeyboardRemapModifierKeyRowElement);
