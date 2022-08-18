// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing Parental Controls features.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from './parental_controls_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsParentalControlsPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsParentalControlsPageElement extends
    SettingsParentalControlsPageElementBase {
  static get is() {
    return 'settings-parental-controls-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      isChild_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isChild');
        },
      },

      /** @private */
      online_: {
        type: Boolean,
        value() {
          return navigator.onLine;
        },
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!ParentalControlsBrowserProxy} */
    this.browserProxy_ = ParentalControlsBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    // Set up online/offline listeners.
    window.addEventListener('offline', this.onOffline_.bind(this));
    window.addEventListener('online', this.onOnline_.bind(this));
  }

  /**
   * Returns the setup parental controls CrButtonElement.
   * @return {?CrButtonElement}
   */
  getSetupButton() {
    return /** @type {?CrButtonElement} */ (
        this.shadowRoot.querySelector('#setupButton'));
  }

  /**
   * Updates the UI when the device goes offline.
   * @private
   */
  onOffline_() {
    this.online_ = false;
  }

  /**
   * Updates the UI when the device comes online.
   * @private
   */
  onOnline_() {
    this.online_ = true;
  }

  /**
   * @return {string} Returns the string to display in the main
   * description area for non-child users.
   * @private
   */
  getSetupLabelText_(online) {
    if (online) {
      return this.i18n('parentalControlsPageSetUpLabel');
    } else {
      return this.i18n('parentalControlsPageConnectToInternetLabel');
    }
  }

  /** @private */
  handleSetupButtonClick_(event) {
    event.stopPropagation();
    this.browserProxy_.showAddSupervisionDialog();
  }

  /** @private */
  handleFamilyLinkButtonClick_(event) {
    event.stopPropagation();
    this.browserProxy_.launchFamilyLinkSettings();
  }
}

customElements.define(
    SettingsParentalControlsPageElement.is,
    SettingsParentalControlsPageElement);
