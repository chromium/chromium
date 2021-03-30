// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-arc-detail-view',

  behaviors: [
    app_management.AppManagementStoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,

    /**
     * @private {boolean}
     */
    listExpanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * @private {boolean}
     */
    isArcSupported_: {
      type: Boolean,
    }
  },

  attached() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.watch('isArcSupported_', state => state.arcSupported);
    this.updateFromStore();

    this.listExpanded_ = false;
  },

  onClickNativeSettingsButton_() {
    app_management.BrowserProxy.getInstance().handler.openNativeSettings(
        this.app_.id);
    app_management.util.recordAppManagementUserAction(
        this.app_.type, AppManagementUserAction.NativeSettingsOpened);
  },

  /**
   * @private
   */
  toggleListExpanded_() {
    this.listExpanded_ = !this.listExpanded_;
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    return app_management.util.getAppIcon(app);
  },

  /**
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  getCollapsedIcon_(listExpanded) {
    return listExpanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * Returns true if the app has not requested any permissions.
   *
   * @param {App} app
   * @return {boolean}
   * @private
   */
  noPermissionsRequested_(app) {
    const permissionItems =
        this.$$('#subpermission-list')
            .querySelectorAll('app-management-permission-item');
    for (let i = 0; i < permissionItems.length; i++) {
      const permissionItem = permissionItems[i];
      const permission =
          app_management.util.getPermission(app, permissionItem.permissionType);
      if (permission !== undefined) {
        return false;
      }
    }
    return true;
  },
});
