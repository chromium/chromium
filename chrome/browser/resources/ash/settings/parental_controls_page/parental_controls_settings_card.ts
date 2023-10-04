// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'parental-controls-settings-card' is the card element for managing Parental
 * Controls features
 */

import 'chrome://resources/cr_elements/icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {isChild} from '../common/load_time_booleans.js';

import {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from './parental_controls_browser_proxy.js';
import {getTemplate} from './parental_controls_settings_card.html.js';

const ParentalControlsSettingsCardElementBase = I18nMixin(PolymerElement);

export class ParentalControlsSettingsCardElement extends
    ParentalControlsSettingsCardElementBase {
  static get is() {
    return 'parental-controls-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isChild_: {
        type: Boolean,
        value() {
          return isChild();
        },
        readOnly: true,
      },

      online_: {
        type: Boolean,
        value() {
          return navigator.onLine;
        },
      },
    };
  }

  private online_: boolean;
  private browserProxy_: ParentalControlsBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = ParentalControlsBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    // Set up online/offline listeners.
    window.addEventListener('offline', this.onOffline_.bind(this));
    window.addEventListener('online', this.onOnline_.bind(this));
  }

  /**
   * Returns the setup parental controls CrButtonElement.
   */
  getSetupButton(): CrButtonElement {
    return castExists(
        this.shadowRoot!.querySelector<CrButtonElement>('#setupButton'));
  }

  /**
   * Updates the UI when the device goes offline.
   */
  private onOffline_(): void {
    this.online_ = false;
  }

  /**
   * Updates the UI when the device comes online.
   */
  private onOnline_(): void {
    this.online_ = true;
  }

  /**
   * @return Returns the string to display in the main
   * description area for non-child users.
   */
  private getSetupLabelText_(online: boolean): string {
    if (online) {
      return this.i18n('parentalControlsPageSetUpLabel');
    }

    return this.i18n('parentalControlsPageConnectToInternetLabel');
  }

  private handleSetupButtonClick_(event: Event): void {
    event.stopPropagation();
    this.browserProxy_.showAddSupervisionDialog();
  }

  private handleFamilyLinkButtonClick_(event: Event): void {
    event.stopPropagation();
    this.browserProxy_.launchFamilyLinkSettings();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ParentalControlsSettingsCardElement.is]:
        ParentalControlsSettingsCardElement;
  }
}

customElements.define(
    ParentalControlsSettingsCardElement.is,
    ParentalControlsSettingsCardElement);
