// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, InstallSource} from 'chrome://resources/cr_components/app_management/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const AppManagementAppDetailsItemBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

class AppManagementAppDetailsItem extends AppManagementAppDetailsItemBase {
  static get is() {
    return 'app-management-app-details-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Object} */
      app: {
        type: Object,
      },

      hidden: {
        type: Boolean,
        computed: 'isHidden_()',
        reflectToAttribute: true,
      },
    };
  }

  /**
   * The supported links item is not available when the feature
   * flag is disabled.
   *
   * @returns {boolean}
   * @private
   */
  isHidden_() {
    return !loadTimeData.getBoolean('appManagementAppDetailsEnabled');
  }

  /**
   * The version is only shown for Android and Chrome apps.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  shouldShowVersion_(app) {
    if (app.version === undefined || app.version === '') {
      return false;
    }

    switch (app.type) {
      case AppType.kChromeApp:
      case AppType.kArc:
        return true;
      default:
        return false;
    }
  }

  /**
   * The full storage information is only shown for
   * Android and Web apps.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  shouldShowStorage_(app) {
    switch (app.type) {
      case AppType.kWeb:
      case AppType.kArc:
      case AppType.kSystemWeb:
        return (app.appSize !== null || app.dataSize !== null);
      default:
        return false;
    }
  }

  /**
   * The app size information is displayed when a value exists.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  shouldShowAppSize_(app) {
    return app.appSize !== null && app.appSize !== '';
  }

  /**
   * The data size information is displayed when a value exists.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  shouldShowDataSize_(app) {
    return app.dataSize !== null && app.dataSize !== '';
  }
  /**
   * The info icon is only shown for apps installed from the Chrome browser.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  shouldShowInfoIcon_(app) {
    return app.installSource === InstallSource.kBrowser;
  }

  /**
   * The launch icon is show for apps installed from the CHrome WEb
   * Store and Google Play Store.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  shouldShowLaunchIcon_(app) {
    return app.dataSize !== null && app.dataSize !== '';
  }

  /**
   * Returns the string for the app type.
   *
   * @param {!App} app
   * @returns {string}
   * @private
   */
  getTypeString_(app) {
    switch (app.type) {
      case AppType.kArc:
        return this.i18n('appManagementAppDetailsTypeAndroid');
      case AppType.kChromeApp:
      case AppType.kStandaloneBrowserChromeApp:
        return this.i18n('appManagementAppDetailsTypeChrome');
      case AppType.kWeb:
      case AppType.kExtension:
      case AppType.kStandaloneBrowserExtension:
        return this.i18n('appManagementAppDetailsTypeWeb');
      case AppType.kBuiltIn:
      case AppType.kSystemWeb:
        return this.i18n('appManagementAppDetailsTypeSystem');
      default:
        console.error('App type not recognised.');
        return '';
    }
  }

  /**
   * Returns the string for the installation source.
   *
   * @param {!App} app
   * @returns {string}
   * @private
   */
  getInstallSourceString_(app) {
    switch (app.installSource) {
      case InstallSource.kChromeWebStore:
        return this.i18n('appManagementAppDetailsInstallSourceWebStore');
      case InstallSource.kPlayStore:
        return this.i18n('appManagementAppDetailsInstallSourcePlayStore');
      default:
        console.error('Install source not recognised.');
        return '';
    }
  }

  /**
   * Returns the string for the app type.
   *
   * @param {!App} app
   * @returns {string}
   * @private
   */
  getTypeAndSourceString_(app) {
    switch (app.installSource) {
      case InstallSource.kSystem:
        return this.i18n('appManagementAppDetailsTypeCrosSystem');
      case InstallSource.kPlayStore:
      case InstallSource.kChromeWebStore:
        return this.i18nAdvanced(
            'appManagementAppDetailsTypeAndSourceCombined', {
              substitutions: [
                String(this.getTypeString_(app)),
                String(this.getInstallSourceString_(app)),
              ],
            });
      case InstallSource.kBrowser:
        return this.i18n('appManagementAppDetailsInstallSourceBrowser');
      case InstallSource.kUnknown:
        return this.getTypeString_(app);
      default:
        console.error('Install source not recognised.');
        return this.getTypeString_(app);
    }
  }

  /**
   * Returns the app size string.
   *
   * @param {!App} app
   * @returns {string}
   * @private
   */
  getAppSizeString_(app) {
    if (app.appSize === null || app.appSize === '') {
      return '';
    }
    return this.i18n(
        'appManagementAppDetailsAppSize', /** @type {!string} */ (app.appSize));
  }

  /**
   * Returns the data size string.
   *
   * @param {!App} app
   * @returns {string}
   * @private
   */
  getDataSizeString_(app) {
    if (app.dataSize === null || app.dataSize === '') {
      return '';
    }
    return this.i18n(
        'appManagementAppDetailsDataSize',
        /** @type {!string} */ (app.dataSize));
  }

  /**
   * Opens the store page for an app when the link is clicked.
   *
   * @param {!Event} e
   * @private
   * @suppress {missingProperties} //TODO(crbug/1315057): Fix closure issue.
   */
  onStoreLinkClicked_(e) {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();

    if (this.app !== null) {
      BrowserProxy.getInstance().handler.openStorePage(this.app.id);
    }
  }

  /**
   * Returns the version string.
   *
   * @param {!App} app
   * @returns {string}
   * @private
   */
  getVersionString_(app) {
    return this.i18n(
        'appManagementAppDetailsVersion',
        app.version ? app.version.toString() : '');
  }

  /**
   * Returns the sanitized URL for apps downloaded from
   * the Chrome browser, to be shown in the tooltip.
   *
   * @param {!App} app
   * @returns {string}
   * @private
   */
  getSanitizedURL_(app) {
    return app.publisherId.replace(/\?.*$/g, '');
  }
}

customElements.define(
    AppManagementAppDetailsItem.is, AppManagementAppDetailsItem);
