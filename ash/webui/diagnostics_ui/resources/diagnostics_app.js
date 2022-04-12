// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './diagnostics_sticky_banner.js';
import './input_list.js';
import './network_list.js';
import './strings.m.js';
import './system_page.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getDiagnosticsIcon, getNavigationIcon} from './diagnostics_utils.js';

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
    /**
     * Used in navigation-view-panel to set show-banner when banner is expected
     * to be shown.
     * @protected
     * @type {string}
     */
    bannerMessage_: {
      type: Boolean,
      value: '',
    },

    /** @protected {boolean} */
    saveSessionLogEnabled_: {
      type: Boolean,
      value: true,
    },

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

    /**
     * Whether a user is logged in or not.
     * Note: A guest session is considered a logged-in state.
     * @protected {boolean}
     */
    isLoggedIn_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isLoggedIn'),
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
      const navPanel = this.$$('#navigationPanel');
      // Note: When adding a new page, update the DiagnosticsPage enum located
      // in chrome/browser/ui/webui/chromeos/diagnostics_dialog.h.
      const pages = [navPanel.createSelectorItem(
          loadTimeData.getString('systemText'), 'system-page',
          getNavigationIcon('laptop-chromebook'), 'system')];

      if (this.isNetworkingEnabled_) {
        pages.push(navPanel.createSelectorItem(
            loadTimeData.getString('connectivityText'), 'network-list',
            getNavigationIcon('ethernet'), 'connectivity'));
      }

      if (this.isInputEnabled_) {
        pages.push(navPanel.createSelectorItem(
            loadTimeData.getString('inputText'), 'input-list',
            getDiagnosticsIcon('keyboard'), 'input'));
      }
      navPanel.addSelectors(pages);
    }
  },

  /** @protected */
  onSessionLogClick_() {
    // Click already handled then leave early.
    if (!this.saveSessionLogEnabled_) {
      return;
    }

    this.saveSessionLogEnabled_ = false;
    this.browserProxy_.saveSessionLog()
        .then(
            /* @type {boolean} */ (success) => {
              const result = success ? 'Success' : 'Failure';
              this.toastText_ =
                  loadTimeData.getString(`sessionLogToastText${result}`);
              this.$.toast.show();
            })
        .catch(() => {/* File selection cancelled */})
        .finally(() => {
          this.saveSessionLogEnabled_ = true;
        });
  },
});
