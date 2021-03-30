// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-plugin-vm-detail-view',

  behaviors: [
    app_management.AppManagementStoreClient,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,

    /**
     * Whether the camera permissions should be shown.
     * @private {boolean}
     */
    showCameraPermissions_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showPluginVmCameraPermissions');
      },
    },

    /**
     * Whether the microphone permissions should be shown.
     * @private {boolean}
     */
    showMicrophonePermissions_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showPluginVmMicrophonePermissions');
      },
    },

    /** @private {boolean} */
    showDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    dialogText_: String,

    /** @private {Element} */
    pendingPermissionItem_: Object,
  },

  attached() {
    // When the state is changed, get the new selected app and assign it to
    // |app_|
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /** @private */
  onSharedPathsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS,
        new URLSearchParams({'id': this.app_.id}));
  },

  /** @private */
  onSharedUsbDevicesClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES,
        new URLSearchParams({'id': this.app_.id}));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onPermissionChanged_: async function(e) {
    this.pendingPermissionItem_ = /** @type {Element} */ (e.target);
    switch (e.target.permissionType) {
      case 'CAMERA':
        this.dialogText_ =
            loadTimeData.getString('pluginVmPermissionDialogCameraLabel');
        break;
      case 'MICROPHONE':
        this.dialogText_ =
            loadTimeData.getString('pluginVmPermissionDialogMicrophoneLabel');
        break;
      default:
        assertNotReached();
    }

    const requiresRelaunch =
        await settings.PluginVmBrowserProxyImpl.getInstance()
            .isRelaunchNeededForNewPermissions();
    if (requiresRelaunch) {
      this.showDialog_ = true;
    } else {
      this.pendingPermissionItem_.syncPermission();
    }
  },

  onRelaunchTap_: function() {
    this.pendingPermissionItem_.syncPermission();
    settings.PluginVmBrowserProxyImpl.getInstance().relaunchPluginVm();
    this.showDialog_ = false;
  },

  onCancel_: function() {
    this.pendingPermissionItem_.resetToggle();
    this.showDialog_ = false;
  },
});
