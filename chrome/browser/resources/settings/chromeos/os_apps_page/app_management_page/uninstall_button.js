// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.m.js';

import {AppManagementUserAction, InstallReason} from '//resources/cr_components/app_management/constants.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getSelectedApp, recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';

import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from '../../metrics_recorder.m.js';

import {BrowserProxy} from './browser_proxy.js';
import {AppManagementStoreClient} from './store_client.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-uninstall-button',

  behaviors: [
    AppManagementStoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,
  },

  attached() {
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * Returns true if the button should be disabled due to app install type.
   *
   * @param {App} app
   * @return {?boolean}
   * @private
   */
  getDisableState_(app) {
    if (!app) {
      return true;
    }

    switch (app.installReason) {
      case InstallReason.kSystem:
      case InstallReason.kPolicy:
        return true;
      case InstallReason.kOem:
      case InstallReason.kDefault:
      case InstallReason.kSync:
      case InstallReason.kUser:
      case InstallReason.kUnknown:
        return false;
      default:
        assertNotReached();
    }
  },

  /**
   * Returns true if the app was installed by a policy.
   *
   * @param {App} app
   * @returns {boolean}
   * @private
   */
  showPolicyIndicator_(app) {
    if (!app) {
      return false;
    }
    return app.installReason === InstallReason.kPolicy;
  },

  /**
   * Returns true if the uninstall button should be shown.
   *
   * @param {App} app
   */
  showUninstallButton_(app) {
    if (!app) {
      return false;
    }
    return app.installReason !== InstallReason.kSystem;
  },

  /**
   * @private
   */
  onClick_() {
    BrowserProxy.getInstance().handler.uninstall(this.app_.id);
    recordSettingChange();
    recordAppManagementUserAction(
        this.app_.type, AppManagementUserAction.UninstallDialogLaunched);
  },
});
