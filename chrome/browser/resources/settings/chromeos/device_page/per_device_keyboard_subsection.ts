// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-keyboard-subsection' allow users to configure their
 * per-device-keyboard subsection settings in system settings.
 */

import '../icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '/shared/settings/controls/settings_radio_group.js';
import '/shared/settings/controls/settings_slider.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import './input_device_settings_shared.css.js';
import './per_device_keyboard_remap_keys.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, Keyboard, KeyboardPolicies, KeyboardSettings, MetaKey} from './input_device_settings_types.js';
import {getPrefPolicyFields, settingsAreEqual} from './input_device_settings_utils.js';
import {getTemplate} from './per_device_keyboard_subsection.html.js';

const SettingsPerDeviceKeyboardSubsectionElementBase =
    DeepLinkingMixin(I18nMixin(RouteOriginMixin(PolymerElement)));

export class SettingsPerDeviceKeyboardSubsectionElement extends
    SettingsPerDeviceKeyboardSubsectionElementBase {
  static get is(): string {
    return 'settings-per-device-keyboard-subsection';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      topRowAreFunctionKeysPref: {
        type: Object,
        value() {
          return {
            key: 'fakeTopRowAreFunctionKeysPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      blockMetaFunctionKeyRewritesPref: {
        type: Object,
        value() {
          return {
            key: 'fakeBlockMetaFunctionKeyRewritesPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      keyboard: {
        type: Object,
      },

      keyboardPolicies: {
        type: Object,
      },

      remapKeyboardKeysSublabel: {
        type: String,
        value: '',
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kKeyboardFunctionKeys,
          Setting.kKeyboardRemapKeys,
        ]),
      },

      keyboardIndex: {
        type: Number,
      },

      isLastDevice: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(topRowAreFunctionKeysPref.value,' +
          'blockMetaFunctionKeyRewritesPref.value,' +
          'enableAutoRepeatPref.value,' +
          'autoRepeatDelaysPref.value,' +
          'autoRepeatIntervalsPref.value)',
      'onPoliciesChanged(keyboardPolicies)',
      'onModifierRemappingsChanged(keyboard.settings.modifierRemappings)',
      'updateSettingsToCurrentPrefs(keyboard)',
    ];
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_KEYBOARD) {
      return;
    }

    if (this.keyboard.isExternal) {
      this.supportedSettingIds.add(Setting.kKeyboardBlockMetaFkeyRewrites);
    }

    // If multiple keyboards are available, focus on the first one.
    if (this.keyboardIndex === 0) {
      this.attemptDeepLink();
    }
  }

  protected keyboard: Keyboard;
  protected keyboardPolicies: KeyboardPolicies;
  private route_: Route = routes.PER_DEVICE_KEYBOARD;
  private topRowAreFunctionKeysPref: chrome.settingsPrivate.PrefObject;
  private blockMetaFunctionKeyRewritesPref: chrome.settingsPrivate.PrefObject;
  private remapKeyboardKeysSublabel: string;
  private isInitialized: boolean = false;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private keyboardIndex: number;
  private isLastDevice: boolean;

  private updateSettingsToCurrentPrefs(): void {
    // `updateSettingsToCurrentPrefs` gets called when the `keyboard` object
    // gets updated. This subsection element can be reused multiple times so we
    // need to reset `isInitialized` so we do not make unneeded API calls.
    this.isInitialized = false;
    this.set(
        'topRowAreFunctionKeysPref.value',
        this.keyboard.settings.topRowAreFkeys);
    this.set(
        'blockMetaFunctionKeyRewritesPref.value',
        this.keyboard.settings.suppressMetaFkeyRewrites);
    this.isInitialized = true;
  }

  private onPoliciesChanged() {
    this.topRowAreFunctionKeysPref = {
      ...this.topRowAreFunctionKeysPref,
      ...getPrefPolicyFields(this.keyboardPolicies.topRowAreFkeysPolicy),
    };
  }

  private onLearnMoreLinkClicked_(event: Event): void {
    const path = event.composedPath();
    if (!Array.isArray(path) || !path.length) {
      return;
    }

    if ((path[0] as HTMLElement).tagName === 'A') {
      // Do not toggle reverse scrolling if the contained link is clicked.
      event.stopPropagation();
    }
  }

  private onSettingsChanged(): void {
    if (!this.isInitialized) {
      return;
    }

    const newSettings: KeyboardSettings = {
      ...this.keyboard.settings,
      topRowAreFkeys: this.topRowAreFunctionKeysPref.value,
      suppressMetaFkeyRewrites: this.blockMetaFunctionKeyRewritesPref.value,
    };

    if (settingsAreEqual(newSettings, this.keyboard.settings)) {
      return;
    }

    this.keyboard.settings = newSettings;
    this.inputDeviceSettingsProvider.setKeyboardSettings(
        this.keyboard.id, this.keyboard.settings);
  }

  private async onModifierRemappingsChanged(): Promise<void> {
    const numRemappedModifierKeys =
        Object.keys(this.keyboard.settings.modifierRemappings).length;

    this.remapKeyboardKeysSublabel =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'remapKeyboardKeysRowSubLabel', numRemappedModifierKeys);
  }

  private onRemapKeyboardKeysTap(): void {
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(this.keyboard.id));

    Router.getInstance().navigateTo(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
  }

  private getKeyboardName(): string {
    return this.keyboard.isExternal ? this.keyboard.name :
                                      this.i18n('builtInKeyboardName');
  }

  private isChromeOsKeyboard(): boolean {
    return this.keyboard.metaKey === MetaKey.kLauncher ||
        this.keyboard.metaKey === MetaKey.kSearch;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-keyboard-subsection':
        SettingsPerDeviceKeyboardSubsectionElement;
  }
}

customElements.define(
    SettingsPerDeviceKeyboardSubsectionElement.is,
    SettingsPerDeviceKeyboardSubsectionElement);
