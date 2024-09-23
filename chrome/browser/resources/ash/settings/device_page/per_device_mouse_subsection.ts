// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-mouse-subsection' allow users to configure their per-device-mouse
 * subsection settings in system settings.
 */

import '../icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_battery_icon_percentage.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import './input_device_settings_shared.css.js';
import './per_device_app_installed_row.js';
import './per_device_install_row.js';
import './per_device_subsection_header.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import {CrLinkRowElement} from 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {CompanionAppState, CustomizationRestriction, InputDeviceSettingsProviderInterface, Mouse, MousePolicies, MouseSettings} from './input_device_settings_types.js';
import {getPrefPolicyFields, settingsAreEqual} from './input_device_settings_utils.js';
import {getTemplate} from './per_device_mouse_subsection.html.js';

const SettingsPerDeviceMouseSubsectionElementBase =
    DeepLinkingMixin(RouteObserverMixin(I18nMixin(PolymerElement)));
export class SettingsPerDeviceMouseSubsectionElement extends
    SettingsPerDeviceMouseSubsectionElementBase {
  static get is() {
    return 'settings-per-device-mouse-subsection';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      isPeripheralCustomizationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePeripheralCustomization');
        },
        readOnly: true,
      },

      primaryRightPref: {
        type: Object,
        value() {
          return {
            key: 'fakePrimaryRightPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      accelerationPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAccelerationPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          };
        },
      },

      sensitivityPref: {
        type: Object,
        value() {
          return {
            key: 'fakeSensitivityPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 3,
          };
        },
      },

      scrollSensitivityPref: {
        type: Object,
        value() {
          return {
            key: 'fakeScrollSensitivityPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 3,
          };
        },
      },

      reverseScrollValue: {
        type: Boolean,
        value: false,
      },

      scrollAccelerationValue: {
        type: Boolean,
        value: true,
      },

      swapPrimaryOptions: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: false,
              name: loadTimeData.getString('primaryMouseButtonLeft'),
            },
            {
              value: true,
              name: loadTimeData.getString('primaryMouseButtonRight'),
            },
          ];
        },
      },

      /**
       * TODO(khorimoto): Remove this conditional once the feature is launched.
       */
      allowScrollSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('allowScrollSettings');
        },
        reflectToAttribute: true,
      },

      /**
       * TODO(michaelpg): settings-slider should optionally take a min and max
       * so we don't have to generate a simple range of natural numbers
       * ourselves. These values match the TouchpadSensitivity enum in
       * enums.xml.
       */
      sensitivityValues_: {
        type: Array,
        value: [1, 2, 3, 4, 5],
        readOnly: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },

      mouse: {
        type: Object,
      },

      mousePolicies: {
        type: Object,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kMouseSwapPrimaryButtons,
          Setting.kMouseReverseScrolling,
          Setting.kMouseAcceleration,
          Setting.kMouseScrollAcceleration,
          Setting.kMouseSpeed,
        ]),
      },

      mouseIndex: {
        type: Number,
      },

      isLastDevice: {
        type: Boolean,
        reflectToAttribute: true,
      },

      customizationRestriction: {
        type: Object,
      },

      /**
         Used to track if the customize button row is clicked.
       */
      currentMouseChanged: {
        type: Boolean,
      },

      isWelcomeExperienceEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableWelcomeExperience');
        },
        readOnly: true,
      },

      deviceImageDataUrl: {
        type: String,
      },

      bluetoothDevice: {
        type: Object,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(primaryRightPref.value,' +
          'accelerationPref.value,' +
          'sensitivityPref.value,' +
          'scrollSensitivityPref.value,' +
          'reverseScrollValue,' +
          'scrollAccelerationValue)',
      'onPoliciesChanged(mousePolicies)',
      'updateSettingsToCurrentPrefs(mouse)',
    ];
  }

  override async currentRouteChanged(route: Route): Promise<void> {
    // Avoid override currentMouseChanged when on the customization subpage.
    if (route === routes.CUSTOMIZE_MOUSE_BUTTONS) {
      return;
    }

    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_MOUSE) {
      // Reset the boolean when on other pages.
      this.currentMouseChanged = false;
      return;
    }

    // If multiple mice are available, focus on the first one.
    if (this.mouseIndex === 0) {
      this.attemptDeepLink();
    }

    // Don't attempt to focus any item unless the last navigation was a
    // 'pop' (backwards) navigation.
    if (!Router.getInstance().lastRouteChangeWasPopstate()) {
      return;
    } else if (this.currentMouseChanged) {
      this.shadowRoot!
          .querySelector<CrLinkRowElement>('#customizeMouseButtons')!.focus();
    }

    this.currentMouseChanged = false;
  }

  isWelcomeExperienceEnabled: boolean;
  private mouse: Mouse;
  protected mousePolicies: MousePolicies;
  private primaryRightPref: chrome.settingsPrivate.PrefObject;
  private accelerationPref: chrome.settingsPrivate.PrefObject;
  private sensitivityPref: chrome.settingsPrivate.PrefObject;
  private scrollSensitivityPref: chrome.settingsPrivate.PrefObject;
  private reverseScrollValue: boolean;
  private scrollAccelerationValue: boolean;
  private isInitialized: boolean = false;
  private isPeripheralCustomizationEnabled_: boolean;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private mouseIndex: number;
  private isLastDevice: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private customizationRestriction: CustomizationRestriction;
  private currentMouseChanged: boolean;

  private showCustomizeButtonRow(): boolean {
    return (this.customizationRestriction !==
            CustomizationRestriction.kDisallowCustomizations) &&
        this.isPeripheralCustomizationEnabled_;
  }

  private showSwapToggleButton(): boolean {
    return this.customizationRestriction ===
        CustomizationRestriction.kDisallowCustomizations &&
        this.isPeripheralCustomizationEnabled_;
  }

  private showInstallAppRow(): boolean {
    return this.mouse.appInfo?.state === CompanionAppState.kAvailable;
  }

  private onInstallCompanionAppButtonClicked(): void {
    window.open(this.mouse.appInfo?.actionLink);
  }

  private updateSettingsToCurrentPrefs(): void {
    // `updateSettingsToCurrentPrefs` gets called when the `keyboard` object
    // gets updated. This subsection element can be reused multiple times so we
    // need to reset `isInitialized` so we do not make unneeded API calls.
    this.isInitialized = false;
    this.set('primaryRightPref.value', this.mouse.settings.swapRight);
    this.set('accelerationPref.value', this.mouse.settings.accelerationEnabled);
    this.set('sensitivityPref.value', this.mouse.settings.sensitivity);
    this.set(
        'scrollSensitivityPref.value', this.mouse.settings.scrollSensitivity);
    this.reverseScrollValue = this.mouse.settings.reverseScrolling;
    this.scrollAccelerationValue = this.mouse.settings.scrollAcceleration;
    this.customizationRestriction = this.mouse.customizationRestriction;
    this.isInitialized = true;
  }

  private onPoliciesChanged(): void {
    this.primaryRightPref = {
      ...this.primaryRightPref,
      ...getPrefPolicyFields(this.mousePolicies.swapRightPolicy),
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

  private onMouseReverseScrollRowClicked_(): void {
    this.reverseScrollValue = !this.reverseScrollValue;
  }

  private onMouseControlledScrollingRowClicked_(): void {
    this.scrollAccelerationValue = !this.scrollAccelerationValue;
  }

  private onSettingsChanged(): void {
    if (!this.isInitialized) {
      return;
    }

    const newSettings: MouseSettings = {
      ...this.mouse.settings,
      swapRight: this.primaryRightPref.value,
      accelerationEnabled: this.accelerationPref.value,
      sensitivity: this.sensitivityPref.value,
      scrollSensitivity: this.scrollSensitivityPref.value,
      reverseScrolling: this.reverseScrollValue,
      scrollAcceleration: this.scrollAccelerationValue,
    };

    if (settingsAreEqual(newSettings, this.mouse.settings)) {
      return;
    }

    this.mouse.settings = newSettings;
    this.inputDeviceSettingsProvider.setMouseSettings(
        this.mouse.id, this.mouse.settings);
  }

  private getLabelWithoutLearnMore(stringName: string): string|TrustedHTML {
    const tempEl = document.createElement('div');
    const localizedString = this.i18nAdvanced(stringName);
    tempEl.innerHTML = localizedString;

    const nodesToDelete: Node[] = [];
    tempEl.childNodes.forEach((node) => {
      // Remove elements with the <a> tag
      if (node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A') {
        nodesToDelete.push(node);
        return;
      }
    });

    nodesToDelete.forEach((node) => {
      tempEl.removeChild(node);
    });

    return tempEl.innerHTML;
  }

  private getCursorSpeedString(): TrustedHTML {
    return this.i18nAdvanced(
        loadTimeData.getBoolean('allowScrollSettings') ? 'cursorSpeed' :
                                                         'mouseSpeed');
  }

  private getCursorAccelerationString(): TrustedHTML {
    return this.i18nAdvanced(
        loadTimeData.getBoolean('allowScrollSettings') ?
            'cursorAccelerationLabel' :
            'mouseAccelerationLabel');
  }

  private onCustomizeButtonsClick(): void {
    const url =
        new URLSearchParams(`mouseId=${encodeURIComponent(this.mouse.id)}`);

    Router.getInstance().navigateTo(
        routes.CUSTOMIZE_MOUSE_BUTTONS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    this.currentMouseChanged = true;
  }

  private getMouseAccelerationDescription(): string {
    if (this.isRevampWayfindingEnabled_) {
      return this.i18n('mouseAccelerationDescription');
    }
    return '';
  }

  private isCompanionAppInstalled(): boolean {
    return this.mouse.appInfo?.state === CompanionAppState.kInstalled;
  }

  private onCompanionAppRowClick(): void {
    assert(this.mouse.appInfo);
    this.inputDeviceSettingsProvider.launchCompanionApp(
        this.mouse.appInfo.packageId || '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-mouse-subsection':
        SettingsPerDeviceMouseSubsectionElement;
  }
}

customElements.define(
    SettingsPerDeviceMouseSubsectionElement.is,
    SettingsPerDeviceMouseSubsectionElement);
