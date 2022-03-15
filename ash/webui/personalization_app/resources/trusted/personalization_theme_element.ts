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

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from './personalization_store.js';
import {setColorModePref} from './theme/theme_controller.js';
import {getThemeProvider} from './theme/theme_interface_provider.js';
import {ThemeObserver} from './theme/theme_observer.js';

export class PersonalizationThemeElement extends WithPersonalizationStore {
  static get is() {
    return 'personalization-theme';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      darkModeEnabled_: Boolean,
    };
  }

  private darkModeEnabled_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    ThemeObserver.initThemeObserverIfNeeded();
    this.watch<PersonalizationThemeElement['darkModeEnabled_']>(
        'darkModeEnabled_', state => state.theme.darkModeEnabled);
    this.updateFromStore();
  }

  private getLightAriaPressed_(darkModeEnabled: boolean) {
    return (!darkModeEnabled).toString();
  }

  private getDarkAriaPressed_(darkModeEnabled: boolean) {
    return darkModeEnabled.toString();
  }

  private getAutoAriaPressed_() {
    // TODO(b/202860714): Add actual implementation.
    return 'false';
  }

  private onClickColorModeButton_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const colorMode = eventTarget.dataset['colorMode'];
    setColorModePref(colorMode === 'DARK', getThemeProvider(), this.getStore());
  }
}

customElements.define(
    PersonalizationThemeElement.is, PersonalizationThemeElement);
