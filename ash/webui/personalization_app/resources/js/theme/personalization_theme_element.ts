// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays color mode settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../../css/common.css.js';
import '../../css/cros_button_style.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {isPersonalizationJellyEnabled} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isSelectionEvent} from '../utils.js';

import {getTemplate} from './personalization_theme_element.html.js';
import {initializeData, setColorModeAutoSchedule, setColorModePref} from './theme_controller.js';
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
      isPersonalizationJellyEnabled_: {
        type: Boolean,
        value() {
          return isPersonalizationJellyEnabled();
        },
      },

      /** The button currently highlighted by keyboard navigation. */
      selectedButton_: {
        type: Object,
        notify: true,
      },
    };
  }

  private darkModeEnabled_: boolean|null;
  private colorModeAutoScheduleEnabled_: boolean|null;
  private selectedButton_: CrButtonElement;

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

  private getLightAriaPressed_(): string {
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

  private getDarkAriaPressed_(): string {
    //  If auto schedule mode is enabled, the system disregards whether dark
    //  mode is enabled or not. To ensure expected behavior, we show that light
    //  mode is selected only when both auto schedule mode and dark mode are
    //  disabled.
    return (!this.colorModeAutoScheduleEnabled_ && !!this.darkModeEnabled_)
        .toString();
  }

  private getAutoAriaPressed_(): string {
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
  }
}

customElements.define(
    PersonalizationThemeElement.is, PersonalizationThemeElement);
