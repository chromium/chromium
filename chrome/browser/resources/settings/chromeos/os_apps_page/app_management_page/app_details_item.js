// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/localized_link/localized_link.js';

import {AppType, InstallSource} from '//resources/cr_components/app_management/constants.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

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
        return this.i18n('appManagementAppDetailsTypeChrome');
      case AppType.kWeb:
      case AppType.kExtension:
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
      case InstallSource.kBrowser:
        return this.i18n('appManagementAppDetailsInstallSourceBrowser');
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
                String(this.getInstallSourceString_(app))
              ]
            });
      case InstallSource.kBrowser:
      case InstallSource.kUnknown:
        return this.getTypeString_(app);
      default:
        console.error('Install source not recognised.');
        return this.getTypeString_(app);
    }
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
}

customElements.define(
    AppManagementAppDetailsItem.is, AppManagementAppDetailsItem);
