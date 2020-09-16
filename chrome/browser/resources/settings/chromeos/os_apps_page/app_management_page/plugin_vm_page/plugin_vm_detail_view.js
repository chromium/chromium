// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-plugin-vm-detail-view',

  behaviors: [
    app_management.StoreClient,
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
    showPluginVmPermissionDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * The current permssion type that is being changed
     * and its proposed value.
     * @private {?PermissionSetting}
     */
    pendingPermissionChange_: {
      type: Object,
      value: null,
    },

    /**
     * If the last permission change should be reset.
     * {boolean}
     */
    resetPermissionChange: {
      type: Boolean,
      value: false,
    },
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
  onPermissionChanged_: function(e) {
    const permissionTypeString =
        /** @type {{permissionType:string}} */ (e.target).permissionType;
    let permissionType;
    switch (permissionTypeString) {
      case 'CAMERA':
        permissionType = PermissionType.CAMERA;
        break;
      case 'MICROPHONE':
        permissionType = PermissionType.MICROPHONE;
        break;
      default:
        assertNotReached();
    }
    const permissionSetting = /** @type {!PermissionSetting} */ ({
      permissionType: permissionType,
      proposedValue: !app_management.util.getPermissionValueBool(
          this.app_, permissionTypeString)
    });
    settings.PluginVmBrowserProxyImpl.getInstance()
        .wouldPermissionChangeRequireRelaunch(permissionSetting)
        .then(requiresRestart => {
          if (requiresRestart) {
            this.pendingPermissionChange_ = permissionSetting;
            this.showPluginVmPermissionDialog_ = true;
          } else {
            settings.PluginVmBrowserProxyImpl.getInstance()
                .setPluginVmPermission(permissionSetting);
          }
        });
  },

  /** @private */
  onPluginVmPermissionDialogClose_: function() {
    if (this.resetPermissionChange) {
      switch (this.pendingPermissionChange_.permissionType) {
        case PermissionType.CAMERA:
          /* @type {!AppManagementPermissionItem} */ (
              this.$$('#camera-permission'))
              .resetToggle();
          break;
        case PermissionType.MICROPHONE:
          /* @type @{!AppManagementPermissionItem} */ (
              this.$$('#microphone-permission'))
              .resetToggle();
          break;
        default:
          assertNotReached();
      }
    }
    this.resetPermissionChange = false;
    this.showPluginVmPermissionDialog_ = false;
    this.pendingPermissionChange_ = null;
  },
});
