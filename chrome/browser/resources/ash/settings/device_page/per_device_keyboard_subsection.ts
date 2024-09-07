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
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import './input_device_settings_shared.css.js';
import './per_device_app_installed_row.js';
import './per_device_install_row.js';
import './per_device_keyboard_remap_keys.js';
import './per_device_subsection_header.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {SettingsSliderElement} from '../controls/settings_slider.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {KeyboardAmbientLightSensorObserverReceiver, KeyboardBrightnessObserverReceiver, LidStateObserverReceiver} from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from '../personalization_page/personalization_hub_browser_proxy.js';
import {Route, Router, routes} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {CompanionAppState, InputDeviceSettingsProviderInterface, Keyboard, KeyboardPolicies, KeyboardSettings, MetaKey, ModifierKey, SixPackKeyInfo, SixPackShortcutModifier} from './input_device_settings_types.js';
import {getPrefPolicyFields, settingsAreEqual} from './input_device_settings_utils.js';
import {getTemplate} from './per_device_keyboard_subsection.html.js';

const SettingsPerDeviceKeyboardSubsectionElementBase =
    DeepLinkingMixin(I18nMixin(RouteObserverMixin(PolymerElement)));

const MIN_VISIBLE_PERCENT = 5;

export class SettingsPerDeviceKeyboardSubsectionElement extends
    SettingsPerDeviceKeyboardSubsectionElementBase {
  static get is() {
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

      keyboardBrightnessPercentPref: {
        type: Object,
        value() {
          return {
            key: 'fakekeyboardBrightnessPercentPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 40,
          };
        },
      },

      keyboardAutoBrightnessPref: {
        type: Object,
        value() {
          return {
            key: 'fakekeyboardAutoBrightnessPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      isKeyboardBacklightControlInSettingsEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableKeyboardBacklightControlInSettings');
        },
        readOnly: true,
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

      isRgbKeyboardSupported: {
        type: Boolean,
        value: false,
      },

      hasKeyboardBacklight: {
        type: Boolean,
        value: false,
      },

      hasAmbientLightSensor: {
        type: Boolean,
        value: false,
      },

      isLidOpen: {
        type: Boolean,
        value: true,
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
      'onKeyboardRemappingsChanged(keyboard.*)',
      'updateSettingsToCurrentPrefs(keyboard)',
    ];
  }

  override currentRouteChanged(newRoute: Route): void {
    // Does not apply to this page.
    if (newRoute !== routes.PER_DEVICE_KEYBOARD) {
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
  private topRowAreFunctionKeysPref: chrome.settingsPrivate.PrefObject;
  private blockMetaFunctionKeyRewritesPref: chrome.settingsPrivate.PrefObject;
  private keyboardBrightnessPercentPref: chrome.settingsPrivate.PrefObject;
  private keyboardAutoBrightnessPref: chrome.settingsPrivate.PrefObject;
  private remapKeyboardKeysSublabel: string;
  private isInitialized: boolean = false;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private personalizationHubBrowserProxy: PersonalizationHubBrowserProxy =
      PersonalizationHubBrowserProxyImpl.getInstance();
  private keyboardBrightnessObserverReceiver:
      KeyboardBrightnessObserverReceiver;
  private keyboardAmbientLightSensorObserverReceiver:
      KeyboardAmbientLightSensorObserverReceiver;
  private lidStateObserverReceiver: LidStateObserverReceiver;
  private keyboardIndex: number;
  private isLastDevice: boolean;
  private isRgbKeyboardSupported: boolean;
  private hasKeyboardBacklight: boolean;
  private hasAmbientLightSensor: boolean;
  private isKeyboardBacklightControlInSettingsEnabled: boolean;
  private isLidOpen: boolean;

  override async connectedCallback(): Promise<void> {
    super.connectedCallback();

    if (this.isKeyboardBacklightControlInSettingsEnabled) {
      // Add keyboardBrightnessChange observer.
      this.keyboardBrightnessObserverReceiver =
          new KeyboardBrightnessObserverReceiver(this);
      this.inputDeviceSettingsProvider.observeKeyboardBrightness(
          this.keyboardBrightnessObserverReceiver.$.bindNewPipeAndPassRemote());

      // Add keyboardAmbientLightSensorChange observer.
      this.keyboardAmbientLightSensorObserverReceiver =
          new KeyboardAmbientLightSensorObserverReceiver(this);
      this.inputDeviceSettingsProvider.observeKeyboardAmbientLightSensor(
          this.keyboardAmbientLightSensorObserverReceiver.$
              .bindNewPipeAndPassRemote());

      // Add LidState Observer.
      this.lidStateObserverReceiver = new LidStateObserverReceiver(this);
      this.inputDeviceSettingsProvider
          .observeLidState(
              this.lidStateObserverReceiver.$.bindNewPipeAndPassRemote())
          .then(({isLidOpen}: {isLidOpen: boolean}) => {
            this.onLidStateChanged(isLidOpen);
          });

      this.isRgbKeyboardSupported =
        (await this.inputDeviceSettingsProvider.isRgbKeyboardSupported())
          ?.isRgbKeyboardSupported;
      this.hasKeyboardBacklight =
          (await this.inputDeviceSettingsProvider.hasKeyboardBacklight())
              ?.hasKeyboardBacklight;
      this.hasAmbientLightSensor =
          (await this.inputDeviceSettingsProvider.hasAmbientLightSensor())
              ?.hasAmbientLightSensor;

      if (this.hasKeyboardBacklight) {
        const crSlider = this.shadowRoot!
                             .querySelector<SettingsSliderElement>(
                                 '#keyboardBrightnessSlider')!.shadowRoot!
                             .querySelector('cr-slider');
        if (crSlider) {
          // Set key press increment value to be 10.
          crSlider.setAttribute('key-press-slider-increment', '10');
        }
      }
    }
  }

  private showInstallAppRow(): boolean {
    return this.keyboard.appInfo?.state === CompanionAppState.kAvailable;
  }

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

  private onPoliciesChanged(): void {
    this.topRowAreFunctionKeysPref = {
      ...this.topRowAreFunctionKeysPref,
      ...getPrefPolicyFields(this.keyboardPolicies.topRowAreFkeysPolicy),
    };
    this.blockMetaFunctionKeyRewritesPref = {
      ...this.blockMetaFunctionKeyRewritesPref,
      ...getPrefPolicyFields(
          this.keyboardPolicies.enableMetaFkeyRewritesPolicy),
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

  private onKeyboardBrightnessSliderChanged(): void {
    this.inputDeviceSettingsProvider.setKeyboardBrightness(
        this.getKeyboardBrightnessFromSlider());
  }

  private onKeyup(event: KeyboardEvent): void {
    // Record updated brightness if adjusted via arrow keys.
    if (['ArrowRight', 'ArrowDown', 'ArrowLeft', 'ArrowUp'].includes(
            event.key)) {
      this.inputDeviceSettingsProvider.recordKeyboardBrightnessChangeFromSlider(
          this.getKeyboardBrightnessFromSlider());
    }
  }

  private onPointerup(): void {
    // Record brightness after slider adjustment is completed.
    this.inputDeviceSettingsProvider.recordKeyboardBrightnessChangeFromSlider(
        this.getKeyboardBrightnessFromSlider());
  }

  private onKeyboardAutoBrightnessToggleChanged(e: Event): void {
    const toggle = e.target as SettingsToggleButtonElement;
    this.inputDeviceSettingsProvider.setKeyboardAmbientLightSensorEnabled(
        toggle.checked);
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

  onKeyboardBrightnessChanged(keyboardBrightnessPercent: number): void {
    if (keyboardBrightnessPercent > 0 &&
        keyboardBrightnessPercent < MIN_VISIBLE_PERCENT) {
      // When auto-brightness is enabled, it's likely that the automated
      // brightness percentage will fall between 0% and 5%. To avoid confusion
      // where the user cannot distinguish between the keyboard being off (0%)
      // and low brightness levels, set the slider to a minimum visible
      // percentage (5%).
      this.set('keyboardBrightnessPercentPref.value', MIN_VISIBLE_PERCENT);
      return;
    }
    this.set('keyboardBrightnessPercentPref.value', keyboardBrightnessPercent);
  }

  onKeyboardAmbientLightSensorEnabledChanged(keyboardAmbientLightSensorEnabled:
                                                 boolean): void {
    this.set(
        'keyboardAutoBrightnessPref.value', keyboardAmbientLightSensorEnabled);
  }

  onLidStateChanged(isLidOpen: boolean): void {
    this.isLidOpen = isLidOpen;
  }

  private getNumRemappedSixPackKeys(): number {
    if (!this.keyboard.settings.sixPackKeyRemappings) {
      return 0;
    }

    return Object
        .values(this.keyboard.settings.sixPackKeyRemappings as SixPackKeyInfo)
        .filter(
            (modifier: SixPackShortcutModifier) =>
                modifier !== SixPackShortcutModifier.kSearch)
        .length;
  }

  private async onKeyboardRemappingsChanged(): Promise<void> {
    let numRemappedKeys =
        Object.keys(this.keyboard.settings.modifierRemappings).length;
    if (loadTimeData.getBoolean('enableAltClickAndSixPackCustomization')) {
      numRemappedKeys += this.getNumRemappedSixPackKeys();
    }
    this.remapKeyboardKeysSublabel =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'remapKeyboardKeysRowSubLabel', numRemappedKeys);
  }

  private onRemapKeyboardKeysClick(): void {
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

  private showKeyboardSettings(): boolean {
    if (!this.isKeyboardBacklightControlInSettingsEnabled) {
      return true;
    }
    return this.keyboard.isExternal ||
        (!this.keyboard.isExternal && this.isLidOpen);
  }

  private isChromeOsKeyboard(): boolean {
    return this.keyboard.metaKey === MetaKey.kLauncher ||
        this.keyboard.metaKey === MetaKey.kSearch ||
        this.keyboard.metaKey === MetaKey.kLauncherRefresh;
  }

  private openPersonalizationHub(): void {
    this.inputDeviceSettingsProvider.recordKeyboardColorLinkClicked();
    this.personalizationHubBrowserProxy.openPersonalizationHub();
  }

  private getKeyboardBrightnessFromSlider(): number {
    const slider = this.shadowRoot!.querySelector<SettingsSliderElement>(
        '#keyboardBrightnessSlider');
    return slider!.pref.value;
  }

  protected getRemapKeyboardKeysClass(): string {
    return `hr bottom-divider ${
        this.keyboard.isExternal ? '' : 'remap-keyboard-keys-row-internal'}`;
  }

  protected showSendFunctionKeyDescription(): string {
    const hasFunctionKey: boolean =
        this.keyboard.modifierKeys.includes(ModifierKey.kFunction);
    if (hasFunctionKey) {
      return this.i18n('splitModifierKeyboardSendFunctionKeysDescription');
    } else {
      return this.i18n('keyboardSendFunctionKeysDescription');
    }
  }

  private isCompanionAppInstalled(): boolean {
    return this.keyboard.appInfo?.state === CompanionAppState.kInstalled;
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
