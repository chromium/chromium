// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './shared_style.js';
import '//resources/cr_components/app_management/shared_vars.js';
import '//resources/cr_elements/cr_icons_css.m.js';

import {AppManagementEntryPoint, AppManagementEntryPointsHistogramName, AppType} from '//resources/cr_components/app_management/constants.js';
import {getAppIcon} from '//resources/cr_components/app_management/util.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {updateSelectedAppId} from './actions.js';
import {AppManagementStoreClient} from './store_client.js';
import {openAppDetailPage} from './util.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-app-item',

  behaviors: [
    AppManagementStoreClient,
  ],

  properties: {
    /** @type {App} */
    app: {
      type: Object,
    },
  },

  listeners: {
    'click': 'onClick_',
  },

  /**
   * @private
   */
  onClick_() {
    openAppDetailPage(this.app.id);
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        this.getAppManagementEntryPoint_(this.app.type),
        Object.keys(AppManagementEntryPoint).length);
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    return getAppIcon(app);
  },

  /**
   * @param {AppType} appType
   * @return {AppManagementEntryPoint}
   */
  getAppManagementEntryPoint_(appType) {
    switch (appType) {
      case AppType.kArc:
        return AppManagementEntryPoint.MainViewArc;
      case AppType.kChromeApp:
      case AppType.kStandaloneBrowser:
      case AppType.kStandaloneBrowserChromeApp:
        // TODO(https://crbug.com/1225848): Figure out appropriate behavior for
        // Lacros-hosted chrome-apps.
        return AppManagementEntryPoint.MainViewChromeApp;
      case AppType.kWeb:
        return AppManagementEntryPoint.MainViewWebApp;
      case AppType.kPluginVm:
        return AppManagementEntryPoint.MainViewPluginVm;
      case AppType.kBorealis:
        return AppManagementEntryPoint.MainViewBorealis;
      default:
        assertNotReached();
    }
  },
});
