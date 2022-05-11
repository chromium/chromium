// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './diagnostics_sticky_banner.js';
import './diagnostics_shared_css.js';
import './input_list.js';
import './network_list.js';
import './strings.m.js';
import './system_page.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {ConnectedDevicesObserverInterface, ConnectedDevicesObserverReceiver, InputDataProviderInterface, KeyboardInfo, TouchDeviceInfo} from './diagnostics_types.js';
import {getDiagnosticsIcon, getNavigationIcon} from './diagnostics_utils.js';
import {getInputDataProvider} from './mojo_interface_provider.js';

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

  /** @private {?InputDataProviderInterface} */
  inputDataProvider_: null,

  /** @private {number} */
  numKeyboards_: -1,

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

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardConnected.
   * @param {!KeyboardInfo} newKeyboard
   */
  onKeyboardConnected(newKeyboard) {
    if (this.numKeyboards_ === 0) {
      this.$.navigationPanel.addSelectorItem(this.createInputSelector_());
    }
    this.numKeyboards_++;
  },

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardDisconnected.
   * @param {number} id
   */
  onKeyboardDisconnected(id) {
    this.numKeyboards_--;
    if (this.numKeyboards_ === 0) {
      this.$.navigationPanel.removeSelectorById('input');
    }
  },

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceConnected.
   * @param {!TouchDeviceInfo} newTouchDevice
   */
  onTouchDeviceConnected(newTouchDevice) {},

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceDisconnected.
   * @param {number} id
   */
  onTouchDeviceDisconnected(id) {},

  /** @private */
  computeShowNavPanel_(isNetworkingEnabled, isInputEnabled) {
    return isNetworkingEnabled || isInputEnabled;
  },

  /** @private */
  createInputSelector_() {
    return this.$.navigationPanel.createSelectorItem(
        loadTimeData.getString('inputText'), 'input-list',
        getDiagnosticsIcon('keyboard'), 'input');
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
        if (loadTimeData.getBoolean('isTouchpadEnabled') ||
            loadTimeData.getBoolean('isTouchscreenEnabled')) {
          pages.push(this.createInputSelector_());
        } else {
          // We only want to show the Input page in the selector if one or more
          // (testable) keyboards are present.
          this.inputDataProvider_ = getInputDataProvider();
          this.inputDataProvider_.getConnectedDevices().then((devices) => {
            this.numKeyboards_ = devices.keyboards.length;
            if (this.numKeyboards_ > 0) {
              navPanel.addSelectorItem(this.createInputSelector_());
            }
          });
          const receiver = new ConnectedDevicesObserverReceiver(
              /** @type {!ConnectedDevicesObserverInterface} */ (this));
          this.inputDataProvider_.observeConnectedDevices(
              receiver.$.bindNewPipeAndPassRemote());
        }
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
