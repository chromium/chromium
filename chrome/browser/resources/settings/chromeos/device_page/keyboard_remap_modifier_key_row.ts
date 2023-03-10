// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'keyboard-remap-key-row' contains a key with icon label and dropdown menu to
 * allow users to customize the remapped key.
 */

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared.css.js';
import '../../controls/settings_dropdown_menu.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import '../os_settings_icons.html.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
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

type MetaKeyIcon = 'cr:search'|'os-settings:launcher'|'';
const KeyboardRemapModifierKeyRowElementBase = I18nMixin(PolymerElement);

export class KeyboardRemapModifierKeyRowElement extends
    KeyboardRemapModifierKeyRowElementBase {
  static get is(): string {
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
        computed: 'computeKeyState(pref.value)',
      },

      pref: {
        type: Object,
      },

      metaKey: {
        type: Number,
        observer: 'onMetaKeyChanged',
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

      metaKeyIcon: {
        type: String,
        value: '',
      },
    };
  }

  protected keyLabel: string;
  private metaKeyLabel: string;
  private keyMapTargets: DropdownMenuOptionList;
  private metaKeyIcon: MetaKeyIcon;
  keyState: KeyState;
  pref: chrome.settingsPrivate.PrefObject;
  metaKey: MetaKey;
  key: ModifierKey;
  defaultRemappings: {[key: number]: ModifierKey};

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

  private onMetaKeyChanged(): void {
    if (this.key === ModifierKey.kMeta) {
      this.metaKeyIcon = this.getMetaKeyIcon();
    }
    this.setUpKeyMapTargets();
  }

  /**
   * Populate the metaKey label required in the keyMapTargets menu dropdown.
   */
  private getMetaKeyLabel(): string {
    switch (this.metaKey) {
      case MetaKey.kCommand: {
        return this.i18n('keyboardKeyCommand');
      }
      case MetaKey.kExternalMeta: {
        return this.i18n('keyboardKeyExternalMeta');
      }
      case MetaKey.kLauncher: {
        return this.i18n('keyboardKeyLauncher');
      }
      case MetaKey.kSearch: {
        return this.i18n('keyboardKeySearch');
      }
    }
  }

  /**
   * Populate the key label inside the keyboard key icon.
   */
  private getKeyLabel(): string {
    switch (this.key) {
      case ModifierKey.kAlt: {
        return this.i18n('keyboardKeyAlt');
      }
      case ModifierKey.kAssistant: {
        return this.i18n('keyboardKeyAssistant');
      }
      case ModifierKey.kBackspace: {
        return this.i18n('keyboardKeyBackspace');
      }
      case ModifierKey.kCapsLock: {
        return this.i18n('keyboardKeyCapsLock');
      }
      case ModifierKey.kControl: {
        return this.i18n('keyboardKeyCtrl');
      }
      case ModifierKey.kEscape: {
        return this.i18n('keyboardKeyEscape');
      }
      case ModifierKey.kMeta: {
        return this.getMetaKeyLabel();
      }
      default:
        assertNotReached('Invalid modifier key: ' + this.key);
    }
  }

  private setUpKeyMapTargets(): void {
    // Ordering is according to UX, but values match ModifierKey.
    this.keyMapTargets = [
      {
        value: ModifierKey.kMeta,
        name: this.metaKeyLabel,
      },
      {
        value: ModifierKey.kControl,
        name: this.i18n('keyboardKeyCtrl'),
      },
      {
        value: ModifierKey.kAlt,
        name: this.i18n('keyboardKeyAlt'),
      },
      {
        value: ModifierKey.kCapsLock,
        name: this.i18n('keyboardKeyCapsLock'),
      },
      {
        value: ModifierKey.kEscape,
        name: this.i18n('keyboardKeyEscape'),
      },
      {
        value: ModifierKey.kBackspace,
        name: this.i18n('keyboardKeyBackspace'),
      },
      {
        value: ModifierKey.kAssistant,
        name: this.i18n('keyboardKeyAssistant'),
      },
      {
        value: ModifierKey.kVoid,
        name: this.i18n('keyboardKeyDisabled'),
      },
    ];
  }

  private getMetaKeyIcon(): MetaKeyIcon {
    if (this.metaKey === MetaKey.kSearch) {
      return 'cr:search';
    }
    if (this.metaKey === MetaKey.kLauncher) {
      return 'os-settings:launcher';
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