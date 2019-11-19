// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  // TODO(crbug.com/999016): change to app-management-chrome-app-detail-view.
  is: 'app-management-chrome-app-permission-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: {
      type: Object,
      observer: 'onAppChanged_',
    },

    /**
     * @private {Array<ExtensionAppPermissionMessage>}
     */
    messages_: Object,
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @private
   */
  onAppChanged_: async function() {
    const {messages: messages} =
        await app_management.BrowserProxy.getInstance()
            .handler.getExtensionAppPermissionMessages(this.app_.id);
    this.messages_ = messages;
  },

  onClickExtensionsSettingsButton_: function() {
    app_management.BrowserProxy.getInstance().handler.openNativeSettings(
        this.app_.id);
    app_management.util.recordAppManagementUserAction(
        this.app_.type, AppManagementUserAction.NativeSettingsOpened);
  },

  /**
   * @param {!Array<ExtensionAppPermissionMessage>} messages
   * @return {Array<string>}
   * @private
   */
  getPermissionMessages_: function(messages) {
    return messages.map(m => m.message);
  },

  /**
   * @param {number} index
   * @param {!Array<ExtensionAppPermissionMessage>} messages
   * @return {?Array<string>}
   * @private
   */
  getPermissionSubmessagesByMessage_: function(index, messages) {
    // Dom-repeat still tries to access messages[0] when app has no
    // permission therefore we add an extra check.
    if (!messages[index]) {
      return null;
    }
    return messages[index].submessages;
  },

  /**
   * @param {!Array<ExtensionAppPermissionMessage>} messages
   * @return {boolean}
   * @private
   */
  hasPermissions_: function(messages) {
    return messages.length > 0;
  },
});
