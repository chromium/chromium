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

import {ThemeObserverInterface, ThemeObserverReceiver, ThemeProviderInterface} from './personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {setDarkModeEnabledAction} from './theme/theme_actions.js';
import {setColorModePref} from './theme/theme_controller.js';
import {getThemeProvider} from './theme/theme_interface_provider.js';

/**
 * Set up the observer to listen for color mode changes.
 */
function initThemeObserver(
    themeProvider: ThemeProviderInterface,
    target: ThemeObserverInterface): ThemeObserverReceiver {
  const receiver = new ThemeObserverReceiver(target);
  themeProvider.setThemeObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}

export class PersonalizationThemeElement extends WithPersonalizationStore
    implements ThemeObserverInterface {
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
  private themeProvider_: ThemeProviderInterface;
  private themeObserver_: ThemeObserverReceiver|null;

  constructor() {
    super();
    this.themeProvider_ = getThemeProvider();
    this.themeObserver_ = null;
  }

  connectedCallback() {
    super.connectedCallback();
    this.themeObserver_ = initThemeObserver(this.themeProvider_, this);
    this.watch<PersonalizationThemeElement['darkModeEnabled_']>(
        'darkModeEnabled_', state => state.theme.darkModeEnabled);
    this.updateFromStore();
  }

  disconnectedCallback() {
    if (this.themeObserver_) {
      this.themeObserver_.$.close();
    }
  }

  onColorModeChanged(darkModeEnabled: boolean) {
    this.dispatch(setDarkModeEnabledAction(darkModeEnabled));
  }

  private getLightAriaPressed_(darkModeEnabled: boolean) {
    return (!darkModeEnabled).toString();
  }

  private getDarkAriaPressed_(darkModeEnabled: boolean) {
    return darkModeEnabled.toString();
  }

  private onClickColorModeButton_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const colorMode = eventTarget.dataset['colorMode'];
    setColorModePref(
        colorMode === 'DARK', this.themeProvider_, this.getStore());
  }
}

customElements.define(
    PersonalizationThemeElement.is, PersonalizationThemeElement);
