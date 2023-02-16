// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-keyboard-settings-remap-keys' displays the remapped keys and
 * allow users to configure their keyboard remapped keys for each keyboard.
 */

import '../../icons.html.js';
import '../../settings_shared.css.js';
import '../../controls/settings_dropdown_menu.js';
import '../../prefs/prefs.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../../controls/settings_dropdown_menu.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, Keyboard, MetaKey, ModifierKey} from './input_device_settings_types.js';
import {getTemplate} from './per_device_keyboard_remap_keys.html.js';

const SettingsPerDeviceKeyboardRemapKeysElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface & RouteObserverMixinInterface,
    };

export class SettingsPerDeviceKeyboardRemapKeysElement extends
    SettingsPerDeviceKeyboardRemapKeysElementBase {
  static get is(): string {
    return 'settings-per-device-keyboard-remap-keys';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      fakeMetaPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.META,
          };
        },
      },

      fakeCtrlPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.CONTROL,
          };
        },
      },

      fakeAltPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.ALT,
          };
        },
      },

      fakeEscPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.ESC,
          };
        },
      },

      fakeBackspacePref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.BACKSPACE,
          };
        },
      },

      fakeAssistantPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.ASSISTANT,
          };
        },
      },

      fakeCapsLockPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.CAPS_LOCK,
          };
        },
      },

      hasAssistantKey: {
        type: Boolean,
        value: false,
      },

      hasCapsLockKey: {
        type: Boolean,
        value: false,
      },

      keyboard: {
        type: Object,
      },

      /** Menu items for key mapping. */
      keyMapTargets: Object,

      metaKeyLabel: {
        type: String,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(fakeMetaPref.value,' +
          'fakeCtrlPref.value,' +
          'fakeAltPref.value,' +
          'fakeEscPref.value,' +
          'fakeBackspacePref.value,' +
          'fakeAssistantPref.value,' +
          'fakeCapsLockPref.value)',
    ];
  }

  protected keyboard: Keyboard;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private keyMapTargets: DropdownMenuOptionList;
  private fakeAltPref: chrome.settingsPrivate.PrefObject;
  private fakeAssistantPref: chrome.settingsPrivate.PrefObject;
  private fakeBackspacePref: chrome.settingsPrivate.PrefObject;
  private fakeCtrlPref: chrome.settingsPrivate.PrefObject;
  private fakeCapsLockPref: chrome.settingsPrivate.PrefObject;
  private fakeEscPref: chrome.settingsPrivate.PrefObject;
  private fakeMetaPref: chrome.settingsPrivate.PrefObject;
  private hasAssistantKey: boolean;
  private hasCapsLockKey: boolean;
  private metaKeyLabel: string;

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_KEYBOARD_REMAP_KEYS) {
      return;
    }
    this.getKeyboard();
  }

  private async getKeyboard(): Promise<void> {
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('keyboardId');
    assert(!!urlSearchQuery);

    // Get the correct keyboard from inputDeviceSettingsProvider with the id.
    const keyboardId = Number(urlSearchQuery);
    const connectedKeyboards =
        await this.inputDeviceSettingsProvider.getConnectedKeyboardSettings();
    const searchedKeyboard =
        connectedKeyboards.find(keyboard => keyboard.id === keyboardId);
    assert(!!searchedKeyboard);
    this.keyboard = searchedKeyboard;
    this.defaultInitializePrefs();

    // Assistant key and caps lock key are optional. Their values depend on
    // keyboard modifierKeys.
    this.hasAssistantKey =
        searchedKeyboard.modifierKeys.includes(ModifierKey.ASSISTANT);
    this.hasCapsLockKey =
        searchedKeyboard.modifierKeys.includes(ModifierKey.CAPS_LOCK);

    // Get MetaKey label from keyboard settings.
    this.metaKeyLabel = this.getMetaKeyLabel();
    this.setUpKeyMapTargets();

    // Update Prefs according to keyboard modifierRemappings.
    Array.from(this.keyboard.settings.modifierRemappings.keys())
        .forEach(originalKey => {
          this.setRemappedKey(originalKey);
        });
  }

  /**
   * Initializes the dropdown menu options for remapping keys.
   */
  private setUpKeyMapTargets() {
    // Ordering is according to UX, but values match ModifierKey.
    this.keyMapTargets = [
      {
        value: ModifierKey.META,
        name: this.metaKeyLabel,
      },
      {
        value: ModifierKey.CONTROL,
        name: 'Ctrl',
      },
      {
        value: ModifierKey.ALT,
        name: 'Alt',
      },
      {
        value: ModifierKey.CAPS_LOCK,
        name: 'Caps Lock',
      },
      {
        value: ModifierKey.ESC,
        name: 'Escape',
      },
      {
        value: ModifierKey.BACKSPACE,
        name: 'Backspace',
      },
      {
        value: ModifierKey.ASSISTANT,
        name: 'Assistant',
      },
      {
        value: ModifierKey.VOID,
        name: 'Disabled',
      },
    ];
  }

  private defaultInitializePrefs(): void {
    this.set('fakeAltPref.value', ModifierKey.ALT);
    this.set('fakeAssitantPref.value', ModifierKey.ASSISTANT);
    this.set('fakeBackspacePref.value', ModifierKey.BACKSPACE);
    this.set('fakeCtrlPref.value', ModifierKey.CONTROL);
    this.set('fakeCapsLockPref.value', ModifierKey.CAPS_LOCK);
    this.set('fakeEscPref.value', ModifierKey.ESC);
    this.set('fakeMetaPref.value', ModifierKey.META);
  }

  private restoreDefaults(): void {
    this.defaultInitializePrefs();
    if (this.keyboard.metaKey === MetaKey.COMMAND) {
      this.set('fakeMetaPref.value', ModifierKey.CONTROL);
      this.set('fakeCtrlPref.value', ModifierKey.META);
    }
  }

  private setRemappedKey(originalKey: ModifierKey): void {
    const targetKey =
        this.keyboard.settings.modifierRemappings.get(originalKey);
    switch (originalKey) {
      case ModifierKey.ALT: {
        this.set('fakeAltPref.value', targetKey);
        break;
      }
      case ModifierKey.ASSISTANT: {
        this.set('fakeAssistantPref.value', targetKey);
        break;
      }
      case ModifierKey.BACKSPACE: {
        this.set('fakeBackspacePref.value', targetKey);
        break;
      }
      case ModifierKey.CAPS_LOCK: {
        this.set('fakeCapsLockPref.value', targetKey);
        break;
      }
      case ModifierKey.CONTROL: {
        this.set('fakeCtrlPref.value', targetKey);
        break;
      }
      case ModifierKey.ESC: {
        this.set('fakeEscPref.value', targetKey);
        break;
      }
      case ModifierKey.META: {
        this.set('fakeMetaPref.value', targetKey);
        break;
      }
    }
  }

  private onSettingsChanged() {
    // TODO(yyhyyh@): Call update keyboard settings API when user changes
    // settings value.
  }

  private getMetaKeyLabel(): string {
    switch (this.keyboard.metaKey) {
      case MetaKey.COMMAND: {
        return this.i18n('keyboardKeyCommand');
      }
      case MetaKey.EXTERNAL_META: {
        return this.i18n('keyboardKeyExternalMeta');
      }
      case MetaKey.LAUNCHER: {
        return this.i18n('keyboardKeyLauncher');
      }
      case MetaKey.SEARCH: {
        return this.i18n('keyboardKeySearch');
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-keyboard-remap-keys':
        SettingsPerDeviceKeyboardRemapKeysElement;
  }
}

customElements.define(
    SettingsPerDeviceKeyboardRemapKeysElement.is,
    SettingsPerDeviceKeyboardRemapKeysElement);
