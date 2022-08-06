// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './storage_external.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, StorageSpaceState} from './device_page_browser_proxy.js';

/**
 * @typedef {{
 *   availableSize: string,
 *   usedSize: string,
 *   usedRatio: number,
 *   spaceState: StorageSpaceState,
 * }}
 */
let StorageSizeStat;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsStorageElementBase = mixinBehaviors(
    [RouteObserverBehavior, RouteOriginBehavior, WebUIListenerBehavior],
    PolymerElement);

/** @polymer */
class SettingsStorageElement extends SettingsStorageElementBase {
  static get is() {
    return 'settings-storage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      androidEnabled: Boolean,

      /** @private */
      showCrostiniStorage_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showCrostini: Boolean,

      /** @private */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      /** @private */
      showOtherUsers_: {
        type: Boolean,
        // Initialize showOtherUsers_ to false if the user is in guest mode.
        value() {
          return !loadTimeData.getBoolean('isGuest');
        },
      },

      /** @private {StorageSizeStat} */
      sizeStat_: Object,
    };
  }

  static get observers() {
    return ['handleCrostiniEnabledChanged_(prefs.crostini.enabled.value)'];
  }

  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.STORAGE;

    /**
     * Timer ID for periodic update.
     * @private {number}
     */
    this.updateTimerId_ = -1;

    /** @private {!DevicePageBrowserProxy} */
    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'storage-size-stat-changed',
        (sizeStat) => this.handleSizeStatChanged_(sizeStat));
    this.addWebUIListener(
        'storage-my-files-size-changed',
        (size) => this.handleMyFilesSizeChanged_(size));
    this.addWebUIListener(
        'storage-browsing-data-size-changed',
        (size) => this.handleBrowsingDataSizeChanged_(size));
    this.addWebUIListener(
        'storage-apps-size-changed',
        (size) => this.handleAppsSizeChanged_(size));
    this.addWebUIListener(
        'storage-crostini-size-changed',
        (size) => this.handleCrostiniSizeChanged_(size));
    if (!this.isGuest_) {
      this.addWebUIListener(
          'storage-other-users-size-changed',
          (size, noOtherUsers) =>
              this.handleOtherUsersSizeChanged_(size, noOtherUsers));
      this.addWebUIListener(
          'storage-system-size-changed',
          (size) => this.handleSystemSizeChanged_(size));
    }
  }

  ready() {
    super.ready();

    const r = routes;
    this.addFocusConfig(r.CROSTINI_DETAILS, '#crostiniSize');
    this.addFocusConfig(r.ACCOUNTS, '#otherUsersSize');
    this.addFocusConfig(
        r.EXTERNAL_STORAGE_PREFERENCES, '#externalStoragePreferences');
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    super.currentRouteChanged(newRoute, oldRoute);

    if (Router.getInstance().getCurrentRoute() !== routes.STORAGE) {
      return;
    }
    this.onPageShown_();
  }

  /** @private */
  onPageShown_() {
    // Updating storage information can be expensive (e.g. computing directory
    // sizes recursively), so we delay this operation until the page is shown.
    this.browserProxy_.updateStorageInfo();
    // We update the storage usage periodically when the overlay is visible.
    this.startPeriodicUpdate_();
  }

  /**
   * Handler for tapping the "My files" item.
   * @private
   */
  onMyFilesTap_() {
    this.browserProxy_.openMyFiles();
  }

  /**
   * Handler for tapping the "Browsing data" item.
   * @private
   */
  onBrowsingDataTap_() {
    window.open('chrome://settings/clearBrowserData');
  }

  /**
   * Handler for tapping the "Apps and Extensions" item.
   * @private
   */
  onAppsTap_() {
    window.location = 'chrome://os-settings/app-management';
  }

  /**
   * Handler for tapping the "Linux storage" item.
   * @private
   */
  onCrostiniTap_() {
    Router.getInstance().navigateTo(
        routes.CROSTINI_DETAILS, /* dynamicParams */ null,
        /* removeSearch */ true);
  }

  /**
   * Handler for tapping the "Other users" item.
   * @private
   */
  onOtherUsersTap_() {
    Router.getInstance().navigateTo(
        routes.ACCOUNTS,
        /* dynamicParams */ null, /* removeSearch */ true);
  }

  /**
   * Handler for tapping the "External storage preferences" item.
   * @private
   */
  onExternalStoragePreferencesTap_() {
    Router.getInstance().navigateTo(routes.EXTERNAL_STORAGE_PREFERENCES);
  }

  /**
   * @param {!StorageSizeStat} sizeStat
   * @private
   */
  handleSizeStatChanged_(sizeStat) {
    this.sizeStat_ = sizeStat;
    this.$.inUseLabelArea.style.width = (sizeStat.usedRatio * 100) + '%';
    this.$.availableLabelArea.style.width =
        ((1 - sizeStat.usedRatio) * 100) + '%';
  }

  /**
   * @param {string} size Formatted string representing the size of My files.
   * @private
   */
  handleMyFilesSizeChanged_(size) {
    this.$.myFilesSize.subLabel = size;
  }

  /**
   * @param {string} size Formatted string representing the size of Browsing
   *     data.
   * @private
   */
  handleBrowsingDataSizeChanged_(size) {
    this.$.browsingDataSize.subLabel = size;
  }

  /**
   * @param {string} size Formatted string representing the size of Apps and
   *     extensions storage.
   * @private
   */
  handleAppsSizeChanged_(size) {
    this.shadowRoot.querySelector('#appsSize').subLabel = size;
  }

  /**
   * @param {string} size Formatted string representing the size of Crostini
   *     storage.
   * @private
   */
  handleCrostiniSizeChanged_(size) {
    if (this.showCrostiniStorage_) {
      this.shadowRoot.querySelector('#crostiniSize').subLabel = size;
    }
  }

  /**
   * @param {string} size Formatted string representing the size of Other
   *     users.
   * @param {boolean} noOtherUsers True if there is no other registered users
   *     on the device.
   * @private
   */
  handleOtherUsersSizeChanged_(size, noOtherUsers) {
    if (this.isGuest_ || noOtherUsers) {
      this.showOtherUsers_ = false;
      return;
    }
    this.showOtherUsers_ = true;
    this.shadowRoot.querySelector('#otherUsersSize').subLabel = size;
  }

  /**
   * @param {string} size Formatted string representing the System size.
   * @private
   */
  handleSystemSizeChanged_(size) {
    this.shadowRoot.querySelector('#systemSizeSubLabel').innerText = size;
  }

  /**
   * @param {boolean} enabled True if Crostini is enabled.
   * @private
   */
  handleCrostiniEnabledChanged_(enabled) {
    this.showCrostiniStorage_ = enabled && this.showCrostini;
  }

  /**
   * Starts periodic update for storage usage.
   * @private
   */
  startPeriodicUpdate_() {
    // We update the storage usage every 5 seconds.
    if (this.updateTimerId_ === -1) {
      this.updateTimerId_ = window.setInterval(() => {
        if (Router.getInstance().getCurrentRoute() !== routes.STORAGE) {
          this.stopPeriodicUpdate_();
          return;
        }
        this.browserProxy_.updateStorageInfo();
      }, 5000);
    }
  }

  /**
   * Stops periodic update for storage usage.
   * @private
   */
  stopPeriodicUpdate_() {
    if (this.updateTimerId_ !== -1) {
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = -1;
    }
  }

  /**
   * Returns true if the remaining space is low, but not critically low.
   * @param {!StorageSpaceState} spaceState Status about the
   *     remaining space.
   * @private
   */
  isSpaceLow_(spaceState) {
    return spaceState === StorageSpaceState.LOW;
  }

  /**
   * Returns true if the remaining space is critically low.
   * @param {!StorageSpaceState} spaceState Status about the
   *     remaining space.
   * @private
   */
  isSpaceCriticallyLow_(spaceState) {
    return spaceState === StorageSpaceState.CRITICALLY_LOW;
  }

  /**
   * Computes class name of the bar based on the remaining space size.
   * @param {!StorageSpaceState} spaceState Status about the
   *     remaining space.
   * @private
   */
  getBarClass_(spaceState) {
    switch (spaceState) {
      case StorageSpaceState.LOW:
        return 'space-low';
      case StorageSpaceState.CRITICALLY_LOW:
        return 'space-critically-low';
      default:
        return '';
    }
  }
}

customElements.define(SettingsStorageElement.is, SettingsStorageElement);
