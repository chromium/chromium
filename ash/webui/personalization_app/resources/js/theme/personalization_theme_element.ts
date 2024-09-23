// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays color mode settings.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import '../geolocation_dialog.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {isCrosPrivacyHubLocationEnabled} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isSelectionEvent} from '../utils.js';

import {getTemplate} from './personalization_theme_element.html.js';
import {enableGeolocationForSystemServices, initializeData, setColorModeAutoSchedule, setColorModePref} from './theme_controller.js';
import {getThemeProvider} from './theme_interface_provider.js';
import {ThemeObserver} from './theme_observer.js';

export interface PersonalizationThemeElement {
  $: {
    keys: IronA11yKeysElement,
    selector: IronSelectorElement,
  };
}

export class PersonalizationThemeElement extends WithPersonalizationStore {
  static get is() {
    return 'personalization-theme';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // null indicates value is being loaded.
      darkModeEnabled_: {
        type: Boolean,
        value: null,
      },

      colorModeAutoScheduleEnabled_: {
        type: Boolean,
        value: null,
      },

      geolocationPermissionEnabled_: {
        type: Boolean,
        value: null,
      },

      geolocationIsUserModifiable_: {
        type: Boolean,
        value: null,
      },

      sunriseTime_: {
        type: String,
        value: null,
      },

      sunsetTime_: {
        type: String,
        value: null,
      },

      /** The button currently highlighted by keyboard navigation. */
      selectedButton_: {
        type: Object,
        notify: true,
      },

      shouldShowGeolocationDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private darkModeEnabled_: boolean|null;
  private colorModeAutoScheduleEnabled_: boolean|null;
  private geolocationPermissionEnabled_: boolean|null;
  private sunriseTime_: string|null;
  private sunsetTime_: string|null;
  private selectedButton_: CrButtonElement;
  private geolocationIsUserModifiable_: boolean|null;
  private shouldShowGeolocationDialog_: boolean;
  private shouldShowGeolocationWarningText_: boolean;

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  override connectedCallback() {
    super.connectedCallback();
    ThemeObserver.initThemeObserverIfNeeded();
    this.watch<PersonalizationThemeElement['darkModeEnabled_']>(
        'darkModeEnabled_', state => state.theme.darkModeEnabled);
    this.watch<PersonalizationThemeElement['colorModeAutoScheduleEnabled_']>(
        'colorModeAutoScheduleEnabled_',
        state => state.theme.colorModeAutoScheduleEnabled);
    this.watch<PersonalizationThemeElement['geolocationPermissionEnabled_']>(
        'geolocationPermissionEnabled_',
        state => state.theme.geolocationPermissionEnabled);
    this.watch<PersonalizationThemeElement['geolocationIsUserModifiable_']>(
        'geolocationIsUserModifiable_',
        state => state.theme.geolocationIsUserModifiable);
    this.watch<PersonalizationThemeElement['sunriseTime_']>(
        'sunriseTime_', state => state.theme.sunriseTime);
    this.watch<PersonalizationThemeElement['sunsetTime_']>(
        'sunsetTime_', state => state.theme.sunsetTime);

    this.updateFromStore();
    initializeData(getThemeProvider(), this.getStore());
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.selector;
    const prevButton = this.selectedButton_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous button.
    if (prevButton) {
      prevButton.removeAttribute('tabindex');
    }
    // Add focus state for new button.
    if (this.selectedButton_) {
      this.selectedButton_.setAttribute('tabindex', '0');
      this.selectedButton_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  private getLightAriaChecked_(): string {
    //  If auto schedule mode is enabled, the system disregards whether dark
    //  mode is enabled or not. To ensure expected behavior, we show that dark
    //  mode can only be selected only when auto schedule mode is disabled and
    //  dark mode is enabled. Also explictly check that these prefs are not null
    //  to avoid showing this button as selected when the values are being
    //  fetched.
    return (this.colorModeAutoScheduleEnabled_ !== null &&
            this.darkModeEnabled_ !== null &&
            !this.colorModeAutoScheduleEnabled_ && !this.darkModeEnabled_)
        .toString();
  }

  private getDarkAriaChecked_(): string {
    //  If auto schedule mode is enabled, the system disregards whether dark
    //  mode is enabled or not. To ensure expected behavior, we show that light
    //  mode is selected only when both auto schedule mode and dark mode are
    //  disabled.
    return (!this.colorModeAutoScheduleEnabled_ && !!this.darkModeEnabled_)
        .toString();
  }

  private getAutoAriaChecked_(): string {
    return (!!this.colorModeAutoScheduleEnabled_).toString();
  }

  private onClickColorModeButton_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    const eventTarget = event.currentTarget as HTMLElement;
    const colorMode = eventTarget.dataset['colorMode'];
    // Disables auto schedule mode if a specific color mode is set.
    setColorModeAutoSchedule(
        /*colorModeAutoScheduleEnabled=*/ false, getThemeProvider(),
        this.getStore());
    setColorModePref(colorMode === 'DARK', getThemeProvider(), this.getStore());
  }

  private onClickAutoModeButton_(event: Event) {
    if (!isSelectionEvent(event) || this.colorModeAutoScheduleEnabled_) {
      return;
    }
    setColorModeAutoSchedule(
        /*enabled=*/ true, getThemeProvider(), this.getStore());

    // If needed, pop up a dialog asking users to enable system location
    // permission.
    if (isCrosPrivacyHubLocationEnabled() &&
        this.geolocationPermissionEnabled_ === false &&
        this.geolocationIsUserModifiable_ === true) {
      this.shouldShowGeolocationDialog_ = true;
    }
  }

  private computeShouldShowTooltipIcon_(): boolean {
    return isCrosPrivacyHubLocationEnabled() &&
        this.colorModeAutoScheduleEnabled_ === true &&
        this.geolocationPermissionEnabled_ === false;
  }

  private computeAutoModeGeolocationDialogText_(): string {
    return loadTimeData.getStringF(
        'autoModeGeolocationDialogText', this.sunriseTime_!, this.sunsetTime_!);
  }

  private onGeolocationDialogClose_(): void {
    this.shouldShowGeolocationDialog_ = false;
  }

  // Callback for user clicking 'Allow' on the geolocation dialog.
  private onGeolocationEnabled_(): void {
    // Enable system geolocation permission for all system services.
    enableGeolocationForSystemServices(getThemeProvider(), this.getStore());
  }
}

customElements.define(
    PersonalizationThemeElement.is, PersonalizationThemeElement);
