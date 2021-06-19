// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
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
    app_management.BrowserProxy.getInstance().handler.setResizeLocked(
        this.app.id,
        newState,
    );
    settings.recordSettingChange();
    const userAction = newState ? AppManagementUserAction.ResizeLockTurnedOn :
                                  AppManagementUserAction.ResizeLockTurnedOff;
    app_management.util.recordAppManagementUserAction(
        this.app.type, userAction);
  },

  /**
   * @private
   */
  onClick_() {
    this.$['toggle-row'].click();
  },
});
