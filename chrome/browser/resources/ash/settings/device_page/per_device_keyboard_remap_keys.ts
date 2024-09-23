// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-keyboard-settings-remap-keys' displays the remapped keys and
 * allow users to configure their keyboard remapped keys for each keyboard.
 */

import '/shared/settings/prefs/prefs.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../controls/settings_dropdown_menu.js';
import './input_device_settings_shared.css.js';
import './fkey_row.js';
import './keyboard_remap_modifier_key_row.js';
import './keyboard_six_pack_key_row.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin, RouteObserverMixinInterface} from '../common/route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {ExtendedFkeysModifier, InputDeviceSettingsFkeyPolicy, InputDeviceSettingsProviderInterface, InputDeviceSettingsSixPackKeyPolicy, Keyboard, KeyboardPolicies, MetaKey, ModifierKey, PolicyStatus, SixPackKey, SixPackKeyInfo, SixPackShortcutModifier} from './input_device_settings_types.js';
import {getTemplate} from './per_device_keyboard_remap_keys.html.js';

interface PrefPolicyFields {
  controlledBy?: chrome.settingsPrivate.ControlledBy;
  enforcement?: chrome.settingsPrivate.Enforcement;
  recommendedValue?: ExtendedFkeysModifier|SixPackShortcutModifier;
}

function getPrefPolicyFields(policy: InputDeviceSettingsFkeyPolicy|
                             InputDeviceSettingsSixPackKeyPolicy|
                             null): PrefPolicyFields {
  if (policy) {
    const enforcement = policy.policyStatus === PolicyStatus.kManaged ?
        chrome.settingsPrivate.Enforcement.ENFORCED :
        chrome.settingsPrivate.Enforcement.RECOMMENDED;
    return {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement,
      recommendedValue: policy.value,
    };
  }
  // These fields must be set back to undefined so the html badge is properly
  // removed from the UI.
  return {
    controlledBy: undefined,
    enforcement: undefined,
    recommendedValue: undefined,
  };
}

const SettingsPerDeviceKeyboardRemapKeysElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface & RouteObserverMixinInterface,
    };

export class SettingsPerDeviceKeyboardRemapKeysElement extends
    SettingsPerDeviceKeyboardRemapKeysElementBase {
  static get is() {
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
            key: 'fakeMetaKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kMeta,
          };
        },
      },

      fakeCtrlPref: {
        type: Object,
        value() {
          return {
            key: 'fakeCtrlKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kControl,
          };
        },
      },

      fakeAltPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kAlt,
          };
        },
      },

      fakeEscPref: {
        type: Object,
        value() {
          return {
            key: 'fakeEscKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kEscape,
          };
        },
      },

      fakeBackspacePref: {
        type: Object,
        value() {
          return {
            key: 'fakeBackspaceKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kBackspace,
          };
        },
      },

      fakeAssistantPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAssistantKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kAssistant,
          };
        },
      },

      fakeCapsLockPref: {
        type: Object,
        value() {
          return {
            key: 'fakeCapsLockKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kCapsLock,
          };
        },
      },

      fakeRightAltPref: {
        type: Object,
        value() {
          return {
            key: 'fakeRightAltKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kRightAlt,
          };
        },
      },

      fakeFunctionPref: {
        type: Object,
        value() {
          return {
            key: 'fakeFunctionKeyRemapPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ModifierKey.kFunction,
          };
        },
      },

      insertPref: {
        type: Object,
        value() {
          return {
            key: 'insertPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: SixPackShortcutModifier.kSearch,
          };
        },
      },

      deletePref: {
        type: Object,
        value() {
          return {
            key: 'deletePref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: SixPackShortcutModifier.kSearch,
          };
        },
      },

      homePref: {
        type: Object,
        value() {
          return {
            key: 'homePref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: SixPackShortcutModifier.kSearch,
          };
        },
      },

      endPref: {
        type: Object,
        value() {
          return {
            key: 'endPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: SixPackShortcutModifier.kSearch,
          };
        },
      },

      pageUpPref: {
        type: Object,
        value() {
          return {
            key: 'pageUpPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: SixPackShortcutModifier.kSearch,
          };
        },
      },

      pageDownPref: {
        type: Object,
        value() {
          return {
            key: 'pageDownPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: SixPackShortcutModifier.kSearch,
          };
        },
      },

      f11KeyPref: {
        type: Object,
        value() {
          return {
            key: 'f11KeyPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ExtendedFkeysModifier.kDisabled,
          };
        },
      },

      f12KeyPref: {
        type: Object,
        value() {
          return {
            key: 'f12KeyPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ExtendedFkeysModifier.kDisabled,
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

      hasRightAltKey: {
        type: Boolean,
        value: false,
      },

      hasFunctionKey: {
        type: Boolean,
        value: false,
      },


      keyboard: {
        type: Object,
      },

      keyboards: {
        type: Array,
        // Prevents the `onKeyboardListUpdated` observer from firing
        // when the page is first initialized.
        value: undefined,
      },

      metaKeyLabel: {
        type: String,
      },

      defaultRemappings: {
        type: Object,
      },

      /**
       * Set it to false when the page is initializing and prefs are being
       * synced to match those in the keyboard's settings from the provider.
       * onSettingsChanged function shouldn't be called during the
       * initialization process.
       */
      isInitialized: {
        type: Boolean,
        value: false,
      },

      keyboardId: {
        type: Number,
        value: -1,
      },

      isAltClickAndSixPackCustomizationEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableAltClickAndSixPackCustomization');
        },
        readOnly: true,
      },

      areF11andF12KeyShortcutsEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableF11AndF12KeyShortcuts');
        },
        readOnly: true,
      },

      keyboardPolicies: {
        type: Object,
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
          'insertPref.value,' +
          'pageUpPref.value,' +
          'pageDownPref.value,' +
          'endPref.value,' +
          'deletePref.value,' +
          'homePref.value,' +
          'f11KeyPref.value,' +
          'f12KeyPref.value,' +
          'fakeRightAltPref.value,' +
          'fakeFunctionPref.value,' +
          'fakeCapsLockPref.value)',
      'onKeyboardListUpdated(keyboards.*)',
      'onPoliciesChanged(keyboardPolicies)',
    ];
  }

  protected get modifierKey(): typeof ModifierKey {
    return ModifierKey;
  }

  keyboard: Keyboard;
  protected keyboardPolicies: KeyboardPolicies;
  isAltClickAndSixPackCustomizationEnabled: boolean;
  areF11andF12KeyShortcutsEnabled: boolean;
  private keyboards: Keyboard[];
  protected keyboardId: number;
  protected defaultRemappings: {[key: number]: ModifierKey} = {
    [ModifierKey.kMeta]: ModifierKey.kMeta,
    [ModifierKey.kControl]: ModifierKey.kControl,
    [ModifierKey.kAlt]: ModifierKey.kAlt,
    [ModifierKey.kEscape]: ModifierKey.kEscape,
    [ModifierKey.kBackspace]: ModifierKey.kBackspace,
    [ModifierKey.kAssistant]: ModifierKey.kAssistant,
    [ModifierKey.kCapsLock]: ModifierKey.kCapsLock,
    [ModifierKey.kRightAlt]: ModifierKey.kRightAlt,
    [ModifierKey.kFunction]: ModifierKey.kFunction,
  };
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private fakeAltPref: chrome.settingsPrivate.PrefObject;
  private fakeAssistantPref: chrome.settingsPrivate.PrefObject;
  private fakeBackspacePref: chrome.settingsPrivate.PrefObject;
  private fakeCtrlPref: chrome.settingsPrivate.PrefObject;
  private fakeCapsLockPref: chrome.settingsPrivate.PrefObject;
  private fakeEscPref: chrome.settingsPrivate.PrefObject;
  private fakeRightAltPref: chrome.settingsPrivate.PrefObject;
  private fakeFunctionPref: chrome.settingsPrivate.PrefObject;
  private fakeMetaPref: chrome.settingsPrivate.PrefObject;
  private insertPref: chrome.settingsPrivate.PrefObject;
  private pageUpPref: chrome.settingsPrivate.PrefObject;
  private pageDownPref: chrome.settingsPrivate.PrefObject;
  private endPref: chrome.settingsPrivate.PrefObject;
  private deletePref: chrome.settingsPrivate.PrefObject;
  private homePref: chrome.settingsPrivate.PrefObject;
  private f11KeyPref: chrome.settingsPrivate.PrefObject;
  private f12KeyPref: chrome.settingsPrivate.PrefObject;
  private hasAssistantKey: boolean;
  private hasCapsLockKey: boolean;
  private hasRightAltKey: boolean;
  private hasFunctionKey: boolean;
  private metaKeyLabel: string;
  private isInitialized: boolean;

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_KEYBOARD_REMAP_KEYS) {
      return;
    }
    if (this.hasKeyboards() &&
        this.keyboardId !== this.getKeyboardIdFromUrl()) {
      this.initializeKeyboard();
    }
  }

  private computeModifierRemappings(): Map<ModifierKey, ModifierKey> {
    const modifierRemappings: Map<ModifierKey, ModifierKey> = new Map();
    for (const modifier of Object.keys(
             this.keyboard.settings.modifierRemappings)) {
      const from: ModifierKey = Number(modifier);
      const to: ModifierKey|undefined =
          this.keyboard.settings.modifierRemappings[from];
      if (to === undefined) {
        continue;
      }
      modifierRemappings.set(from, to);
    }
    return modifierRemappings;
  }

  /**
   * Get the keyboard to display according to the keyboardId in the url query,
   * initializing the page and pref with the keyboard data.
   */
  private initializeKeyboard(): void {
    // Set isInitialized to false to prevent calling update keyboard settings
    // api while the prefs are initializing.
    this.isInitialized = false;
    this.keyboardId = this.getKeyboardIdFromUrl();
    const searchedKeyboard = this.keyboards.find(
        (keyboard: Keyboard) => keyboard.id === this.keyboardId);
    assert(!!searchedKeyboard);
    this.keyboard = searchedKeyboard;
    this.updateDefaultRemapping();
    this.initializePrefsToIdentity();

    // Assistant key and caps lock key are optional. Their values depend on
    // keyboard modifierKeys.
    this.hasAssistantKey =
        searchedKeyboard.modifierKeys.includes(ModifierKey.kAssistant);
    this.hasCapsLockKey =
        searchedKeyboard.modifierKeys.includes(ModifierKey.kCapsLock);
    this.hasRightAltKey =
        searchedKeyboard.modifierKeys.includes(ModifierKey.kRightAlt);
    this.hasFunctionKey =
        searchedKeyboard.modifierKeys.includes(ModifierKey.kFunction);

    // Update Prefs according to keyboard modifierRemappings.
    Array.from(this.computeModifierRemappings().keys())
        .forEach((originalKey: ModifierKey) => {
          this.setRemappedKey(originalKey);
        });

    if (this.isAltClickAndSixPackCustomizationEnabled) {
      this.setSixPackKeyRemappings();

      // Potentially overrides some/all "six pack" settings based on
      // the keyboard policies.
      this.setSixPackKeyRemappingsForPolicies();
    }

    if (this.shouldShowFkeys()) {
      this.set('f11KeyPref.value', searchedKeyboard.settings?.f11);
      this.set('f12KeyPref.value', searchedKeyboard.settings?.f12);
      this.f11KeyPref = {
        ...this.f11KeyPref,
        ...getPrefPolicyFields(this.keyboardPolicies?.f11KeyPolicy),
      };
      this.f12KeyPref = {
        ...this.f12KeyPref,
        ...getPrefPolicyFields(this.keyboardPolicies?.f12KeyPolicy),
      };
    }

    this.isInitialized = true;
  }

  private keyboardWasDisconnected(id: number): boolean {
    return !this.keyboards.find(keyboard => keyboard.id === id);
  }

  onKeyboardListUpdated(): void {
    if (Router.getInstance().currentRoute !==
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS) {
      return;
    }

    if (!this.hasKeyboards() ||
        this.keyboardWasDisconnected(this.getKeyboardIdFromUrl())) {
      this.keyboardId = -1;
      Router.getInstance().navigateTo(routes.PER_DEVICE_KEYBOARD);
      return;
    }
    this.initializeKeyboard();
  }

  setSixPackKeyRemappingsForPolicies(): void {
    const homeAndEndPrefPolicyFields =
        getPrefPolicyFields(this.keyboardPolicies?.homeAndEndKeysPolicy);
    this.homePref = {...this.homePref, ...homeAndEndPrefPolicyFields};
    this.endPref = {...this.endPref, ...homeAndEndPrefPolicyFields};
    const pageUpAndPageDownPrefPolicyFields =
        getPrefPolicyFields(this.keyboardPolicies?.pageUpAndPageDownKeysPolicy);
    this.pageUpPref = {
      ...this.pageUpPref,
      ...pageUpAndPageDownPrefPolicyFields,
    };
    this.pageDownPref = {
      ...this.pageDownPref,
      ...pageUpAndPageDownPrefPolicyFields,
    };
    this.deletePref = {
      ...this.deletePref,
      ...getPrefPolicyFields(this.keyboardPolicies?.deleteKeyPolicy),
    };
    this.insertPref = {
      ...this.insertPref,
      ...getPrefPolicyFields(this.keyboardPolicies?.insertKeyPolicy),
    };
  }

  /**
   * Sets all prefs to the "identity" value which so they can be updated by the
   * values in the remappings map.
   */
  private initializePrefsToIdentity(): void {
    this.set('fakeAltPref.value', ModifierKey.kAlt);
    this.set('fakeAssitantPref.value', ModifierKey.kAssistant);
    this.set('fakeBackspacePref.value', ModifierKey.kBackspace);
    this.set('fakeCtrlPref.value', ModifierKey.kControl);
    this.set('fakeCapsLockPref.value', ModifierKey.kCapsLock);
    this.set('fakeEscPref.value', ModifierKey.kEscape);
    this.set('fakeMetaPref.value', ModifierKey.kMeta);
    if (loadTimeData.getBoolean('enableModifierSplit')) {
      this.set('fakeRightAltPref.value', ModifierKey.kRightAlt);
    }
    if (this.hasFunctionKey) {
      this.set('fakeFunctionPref.value', ModifierKey.kFunction);
    }
  }

  restoreDefaults(): void {
    this.inputDeviceSettingsProvider.restoreDefaultKeyboardRemappings(
        this.keyboardId);
  }

  private setRemappedKey(originalKey: ModifierKey): void {
    const targetKey = this.computeModifierRemappings().get(originalKey);
    switch (originalKey) {
      case ModifierKey.kAlt: {
        this.set('fakeAltPref.value', targetKey);
        break;
      }
      case ModifierKey.kAssistant: {
        this.set('fakeAssistantPref.value', targetKey);
        break;
      }
      case ModifierKey.kBackspace: {
        this.set('fakeBackspacePref.value', targetKey);
        break;
      }
      case ModifierKey.kCapsLock: {
        this.set('fakeCapsLockPref.value', targetKey);
        break;
      }
      case ModifierKey.kControl: {
        this.set('fakeCtrlPref.value', targetKey);
        break;
      }
      case ModifierKey.kEscape: {
        this.set('fakeEscPref.value', targetKey);
        break;
      }
      case ModifierKey.kMeta: {
        this.set('fakeMetaPref.value', targetKey);
        break;
      }
      case ModifierKey.kRightAlt: {
        this.set('fakeRightAltPref.value', targetKey);
        break;
      }
      case ModifierKey.kFunction: {
        this.set('fakeFunctionPref.value', targetKey);
        break;
      }
    }
  }

  /**
   * Update keyboard settings when the prefs change.
   */
  private onSettingsChanged(): void {
    if (!this.isInitialized) {
      return;
    }

    this.keyboard.settings = {
      ...this.keyboard.settings,
      modifierRemappings: this.getUpdatedRemappings(),
    };

    if (this.isAltClickAndSixPackCustomizationEnabled) {
      this.keyboard.settings = {
        ...this.keyboard.settings,
        sixPackKeyRemappings: this.getSixPackKeyRemappings(),
      };
    }

    if (this.shouldShowFkeys()) {
      this.keyboard.settings = {
        ...this.keyboard.settings,
        f11: this.f11KeyPref.value,
        f12: this.f12KeyPref.value,
      };
    }

    this.inputDeviceSettingsProvider.setKeyboardSettings(
        this.keyboard.id, this.keyboard.settings);
  }

  /**
   * Get the modifier remappings with updated pref values.
   */
  private getUpdatedRemappings(): {[key: number]: ModifierKey} {
    const updatedRemappings: {[key: number]: ModifierKey} = {};

    if (ModifierKey.kAlt !== this.fakeAltPref.value) {
      updatedRemappings[ModifierKey.kAlt] = this.fakeAltPref.value;
    }
    if (ModifierKey.kAssistant !== this.fakeAssistantPref.value) {
      updatedRemappings[ModifierKey.kAssistant] = this.fakeAssistantPref.value;
    }
    if (ModifierKey.kBackspace !== this.fakeBackspacePref.value) {
      updatedRemappings[ModifierKey.kBackspace] = this.fakeBackspacePref.value;
    }
    if (ModifierKey.kCapsLock !== this.fakeCapsLockPref.value) {
      updatedRemappings[ModifierKey.kCapsLock] = this.fakeCapsLockPref.value;
    }
    if (ModifierKey.kControl !== this.fakeCtrlPref.value) {
      updatedRemappings[ModifierKey.kControl] = this.fakeCtrlPref.value;
    }
    if (ModifierKey.kEscape !== this.fakeEscPref.value) {
      updatedRemappings[ModifierKey.kEscape] = this.fakeEscPref.value;
    }
    if (ModifierKey.kMeta !== this.fakeMetaPref.value) {
      updatedRemappings[ModifierKey.kMeta] = this.fakeMetaPref.value;
    }

    if (loadTimeData.getBoolean('enableModifierSplit')) {
      if (ModifierKey.kRightAlt !== this.fakeRightAltPref.value) {
        updatedRemappings[ModifierKey.kRightAlt] = this.fakeRightAltPref.value;
      }
    }

    if (this.hasFunctionKey) {
      if (ModifierKey.kFunction !== this.fakeFunctionPref.value) {
        updatedRemappings[ModifierKey.kFunction] = this.fakeFunctionPref.value;
      }
    }

    return updatedRemappings;
  }

  private updateDefaultRemapping(): void {
    this.defaultRemappings = {
      ...this.defaultRemappings,
      [ModifierKey.kMeta]:
          this.keyboard.metaKey === MetaKey.kCommand ? ModifierKey.kControl :
                                                       ModifierKey.kMeta,
      [ModifierKey.kControl]:
          this.keyboard.metaKey === MetaKey.kCommand ? ModifierKey.kMeta :
                                                       ModifierKey.kControl,
    };
  }

  private getKeyboardIdFromUrl(): number {
    return Number(Router.getInstance().getQueryParameters().get('keyboardId'));
  }

  private hasKeyboards(): boolean {
    return this.keyboards?.length > 0;
  }

  private computeKeyboardKeysDescription(): string {
    if (!this.keyboard?.name) {
      return '';
    }
    const keyboardName = this.keyboard.isExternal ?
        this.keyboard.name :
        this.i18n('builtInKeyboardName');

    if (this.isAltClickAndSixPackCustomizationEnabled) {
      return keyboardName;
    }

    return this.i18n('remapKeyboardKeysDescription', keyboardName);
  }

  private setSixPackKeyRemappings(): void {
    const sixPackKeyRemappings: SixPackKeyInfo|null =
        this.keyboard.settings?.sixPackKeyRemappings;
    if (!sixPackKeyRemappings) {
      return;
    }
    Object.entries(sixPackKeyRemappings).forEach(([key, modifier]) => {
      switch (key) {
        case SixPackKey.DELETE:
          this.set('deletePref.value', modifier);
          break;
        case SixPackKey.INSERT:
          this.set('insertPref.value', modifier);
          break;
        case SixPackKey.HOME:
          this.set('homePref.value', modifier);
          break;
        case SixPackKey.END:
          this.set('endPref.value', modifier);
          break;
        case SixPackKey.PAGE_UP:
          this.set('pageUpPref.value', modifier);
          break;
        case SixPackKey.PAGE_DOWN:
          this.set('pageDownPref.value', modifier);
          break;
      }
    });
  }

  private getSixPackKeyRemappings(): SixPackKeyInfo {
    return {
      home: this.homePref.value,
      pageUp: this.pageUpPref.value,
      pageDown: this.pageDownPref.value,
      del: this.deletePref.value,
      insert: this.insertPref.value,
      end: this.endPref.value,
    };
  }

  protected shouldShowFkeys(): boolean {
    return this.areF11andF12KeyShortcutsEnabled &&
        (this.keyboard?.settings?.f11 != null &&
         this.keyboard?.settings?.f12 != null) &&
        !this.hasFunctionKey;
  }

  private onPoliciesChanged(): void {
    if (this.shouldShowFkeys()) {
      this.f11KeyPref = {
        ...this.f11KeyPref,
        ...getPrefPolicyFields(this.keyboardPolicies?.f11KeyPolicy),
      };
      this.f12KeyPref = {
        ...this.f12KeyPref,
        ...getPrefPolicyFields(this.keyboardPolicies?.f12KeyPolicy),
      };
    }

    this.setSixPackKeyRemappingsForPolicies();
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
