// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from '../../metrics_recorder.m.js';

import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {recordAppManagementUserAction} from './util.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-resize-lock-item',

  properties: {
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
  },

  listeners: {
    click: 'onClick_',
    change: 'toggleSetting_',
  },

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
  },

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
  },

  toggleSetting_() {
    const newState = !this.app.resizeLocked;
    assert(newState === this.$['toggle-row'].isChecked());
    BrowserProxy.getInstance().handler.setResizeLocked(
        this.app.id,
        newState,
    );
    recordSettingChange();
    const userAction = newState ? AppManagementUserAction.ResizeLockTurnedOn :
                                  AppManagementUserAction.ResizeLockTurnedOff;
    recordAppManagementUserAction(this.app.type, userAction);
  },

  /**
   * @private
   */
  onClick_() {
    this.$['toggle-row'].click();
  },
});
