// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './shared_style.js';
import './shared_vars.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';

import {AppManagementEntryPoint, AppManagementEntryPointsHistogramName, AppType} from 'chrome://resources/cr_components/app_management/constants.js';
import {getAppIcon} from 'chrome://resources/cr_components/app_management/util.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreClient, AppManagementStoreClientInterface} from './store_client.js';
import {openAppDetailPage} from './util.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AppManagementStoreClientInterface}
 */
const AppManagementAppItemElementBase =
    mixinBehaviors([AppManagementStoreClient], PolymerElement);

/** @polymer */
class AppManagementAppItemElement extends AppManagementAppItemElementBase {
  static get is() {
    return 'app-management-app-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {App} */
      app: {
        type: Object,
      },
    };
  }

  ready() {
    super.ready();

    this.addEventListener('click', this.onClick_);
  }

  /**
   * @private
   */
  onClick_() {
    openAppDetailPage(this.app.id);
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        this.getAppManagementEntryPoint_(this.app.type),
        Object.keys(AppManagementEntryPoint).length);
  }

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    return getAppIcon(app);
  }

  /**
   * @param {appManagement.mojom.AppType} appType
   * @return {AppManagementEntryPointType}
   */
  getAppManagementEntryPoint_(appType) {
    switch (appType) {
      case AppType.kArc:
        return AppManagementEntryPoint.MAIN_VIEW_ARC;
      case AppType.kChromeApp:
      case AppType.kStandaloneBrowser:
      case AppType.kStandaloneBrowserChromeApp:
        // TODO(https://crbug.com/1225848): Figure out appropriate behavior for
        // Lacros-hosted chrome-apps.
        return AppManagementEntryPoint.MAIN_VIEW_CHROME_APP;
      case AppType.kWeb:
        return AppManagementEntryPoint.MAIN_VIEW_WEB_APP;
      case AppType.kPluginVm:
        return AppManagementEntryPoint.MAIN_VIEW_PLUGIN_VM;
      case AppType.kBorealis:
        return AppManagementEntryPoint.MAIN_VIEW_BOREALIS;
      default:
        assertNotReached();
    }
  }
}

customElements.define(
    AppManagementAppItemElement.is, AppManagementAppItemElement);
