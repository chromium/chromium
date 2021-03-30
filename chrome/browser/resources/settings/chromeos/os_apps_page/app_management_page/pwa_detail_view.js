// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-pwa-detail-view',

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
  },

  attached() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();

    this.listExpanded_ = false;
  },

  /**
   * @private
   */
  onClickSiteSettingsButton_() {
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
});
