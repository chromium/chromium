// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './input_list.js';
import './network_list.js';
import './strings.m.js';
import './system_page.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';

/**
 * @fileoverview
 * 'diagnostics-app' is responsible for displaying the 'system-page' which is
 * the main page for viewing telemetric system information and running
 * diagnostic tests.
 */
Polymer({
  is: 'diagnostics-app',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?DiagnosticsBrowserProxy} */
  browserProxy_: null,

  properties: {
    /** @private {boolean} */
    showNavPanel_: {
      type: Boolean,
      computed: 'computeShowNavPanel_(isNetworkingEnabled_, isInputEnabled_)',
    },

    /** @private {boolean} */
    isNetworkingEnabled_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isNetworkingEnabled'),
    },

    /** @private {boolean} */
    isInputEnabled_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isInputEnabled'),
    },

    /** @private {string} */
    toastText_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  created() {
    this.browserProxy_ = DiagnosticsBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();
  },

  /** @private */
  computeShowNavPanel_(isNetworkingEnabled, isInputEnabled) {
    return isNetworkingEnabled || isInputEnabled;
  },

  /** @override */
  attached() {
    if (this.showNavPanel_) {
      this.$$('#navigationPanel')
          .addSelector(
              loadTimeData.getString('overviewText'), 'system-page',
              'navigation-selector:laptop-chromebook');
      if (this.isNetworkingEnabled_) {
        this.$$('#navigationPanel')
            .addSelector(
                loadTimeData.getString('connectivityText'), 'network-list',
                'navigation-selector:ethernet');
      }
      if (this.isInputEnabled_) {
        this.$$('#navigationPanel').addSelector('Input', 'input-list');
      }
    }
  },

  /** @protected */
  onSessionLogClick_() {
    this.browserProxy_.saveSessionLog()
        .then(
            /* @type {boolean} */ (success) => {
              const result = success ? 'Success' : 'Failure';
              this.toastText_ =
                  loadTimeData.getString(`sessionLogToastText${result}`);
              this.$.toast.show();
            })
        .catch(() => {/* File selection cancelled */});
  },
});
