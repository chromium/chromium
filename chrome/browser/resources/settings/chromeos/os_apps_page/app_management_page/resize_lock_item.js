// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/app_management/toggle_row.js';

import {AppManagementUserAction} from '//resources/cr_components/app_management/constants.js';
import {assert} from '//resources/js/assert.m.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';

import {recordSettingChange} from '../../metrics_recorder.js';

import {BrowserProxy} from './browser_proxy.js';

/** @polymer */
class AppManagementResizeLockItemElement extends PolymerElement {
  static get is() {
    return 'app-management-resize-lock-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {App}
       */
      app: Object,

      /**
       * @type {boolean}
       */
      hidden: {
        type: Boolean,
        computed: 'isHidden_(app)',
        reflectToAttribute: true,
      },
    };
  }

  ready() {
    super.ready();

    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleSetting_);
  }

  /**
   * @param {App} app
   * @returns {boolean} true if the app is resize locked.
   * @private
   */
  getValue_(app) {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.resizeLocked;
  }

  /**
   * @param {App} app
   * @returns {boolean} true if resize lock setting is hidden.
   */
  isHidden_(app) {
    if (app === undefined) {
      return true;
    }
    assert(app);
    return app.hideResizeLocked;
  }

  /** @private */
  toggleSetting_() {
    const newState = !this.app.resizeLocked;
    assert(newState === this.$['toggle-row'].isChecked());
    BrowserProxy.getInstance().handler.setResizeLocked(
        this.app.id,
        newState,
    );
    recordSettingChange();
    const userAction = newState ?
        AppManagementUserAction.RESIZE_LOCK_TURNED_ON :
        AppManagementUserAction.RESIZE_LOCK_TURNED_OFF;
    recordAppManagementUserAction(this.app.type, userAction);
  }

  /** @private */
  onClick_() {
    this.$['toggle-row'].click();
  }
}

customElements.define(
    AppManagementResizeLockItemElement.is, AppManagementResizeLockItemElement);
