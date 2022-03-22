// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing Parental Controls features.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/icons.m.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';

import {addWebUIListener, removeWebUIListener, sendWithPromise, WebUIListener} from '//resources/js/cr.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {routes} from '../os_route.js';

import {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from './parental_controls_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-parental-controls-page',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @private */
    isChild_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isChild');
      }
    },

    /** @private */
    online_: {
      type: Boolean,
      value() {
        return navigator.onLine;
      }
    },
  },

  /** @override */
  created() {
    this.browserProxy_ = ParentalControlsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    // Set up online/offline listeners.
    window.addEventListener('offline', this.onOffline_.bind(this));
    window.addEventListener('online', this.onOnline_.bind(this));
  },

  /**
   * Returns the setup parental controls CrButtonElement.
   * @return {?CrButtonElement}
   */
  getSetupButton() {
    return /** @type {?CrButtonElement} */ (this.$$('#setupButton'));
  },

  /**
   * Updates the UI when the device goes offline.
   * @private
   */
  onOffline_() {
    this.online_ = false;
  },

  /**
   * Updates the UI when the device comes online.
   * @private
   */
  onOnline_() {
    this.online_ = true;
  },

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
  },

  /** @private */
  handleSetupButtonClick_(event) {
    event.stopPropagation();
    this.browserProxy_.showAddSupervisionDialog();
  },

  /** @private */
  handleFamilyLinkButtonClick_(event) {
    event.stopPropagation();
    this.browserProxy_.launchFamilyLinkSettings();
  },
});
