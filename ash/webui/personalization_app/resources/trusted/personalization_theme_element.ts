// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays color mode settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../common/styles.js';
import './cros_button_style.js';

import {isSelectionEvent} from '../common/utils.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {getTemplate} from './personalization_theme_element.html.js';
import {initializeData, setColorModeAutoSchedule, setColorModePref} from './theme/theme_controller.js';
import {getThemeProvider} from './theme/theme_interface_provider.js';
import {ThemeObserver} from './theme/theme_observer.js';

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
      darkModeEnabled_: Boolean,
      colorModeAutoScheduleEnabled_: Boolean,
      loading_: {
        type: Boolean,
        computed:
            'computeLoading_(darkModeEnabled_, colorModeAutoScheduleEnabled_)',
      },
    };
  }

  private darkModeEnabled_: boolean|null = null;
  private colorModeAutoScheduleEnabled_: boolean|null = null;
  private loading_: boolean;

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

  private getLightAriaPressed_(): string {
    //  If auto schedule mode is enabled, the system disregards whether dark
    //  mode is enabled or not. To ensure expected behavior, we show that dark
    //  mode can only be selected only when auto schedule mode is disabled and
    //  dark mode is enabled.
    return (!this.colorModeAutoScheduleEnabled_ && !this.darkModeEnabled_)
        .toString();
  }

  private getDarkAriaPressed_() {
    //  If auto schedule mode is enabled, the system disregards whether dark
    //  mode is enabled or not. To ensure expected behavior, we show that light
    //  mode is selected only when both auto schedule mode and dark mode are
    //  disabled.
    return (!this.colorModeAutoScheduleEnabled_ && !!this.darkModeEnabled_)
        .toString();
  }

  private getAutoAriaPressed_() {
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
    if (!isSelectionEvent(event)) {
      return;
    }
    setColorModeAutoSchedule(
        !this.colorModeAutoScheduleEnabled_, getThemeProvider(),
        this.getStore());
  }

  private computeLoading_() {
    return this.colorModeAutoScheduleEnabled_ === null ||
        this.darkModeEnabled_ === null;
  }
}

customElements.define(
    PersonalizationThemeElement.is, PersonalizationThemeElement);
