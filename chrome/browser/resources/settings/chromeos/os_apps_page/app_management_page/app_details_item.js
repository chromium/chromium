// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/localized_link/localized_link.js';

import {AppType, InstallSource} from '//resources/cr_components/app_management/constants.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

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
      app: appManagement.mojom.App,

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
    if (app.installSource === InstallSource.kSystem) {
      return this.i18n('appManagementAppDetailsTypeCrosSystem');
    } else if (app.installSource === InstallSource.kUnknown) {
      return this.getTypeString_(app);
    }
    return this.i18n(
        'appManagementAppDetailsTypeAndSourceCombined',
        this.getTypeString_(app), this.getInstallSourceString_(app));
  }
}

customElements.define(
    AppManagementAppDetailsItem.is, AppManagementAppDetailsItem);
