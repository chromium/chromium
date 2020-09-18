// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-shared-usb-devices' is the settings shared usb devices subpage for
 * Plugin VM.
 */

Polymer({
  is: 'settings-plugin-vm-shared-usb-devices',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The USB Devices available for connection to a VM.
     * @private {Array<!PluginVmSharedUsbDevice>}
     */
    sharedUsbDevices_: Array,

    /**
     * The USB device which was toggled to be shared, but is already shared
     * with another VM. When non-null the reassign dialog is shown.
     * @private {?PluginVmSharedUsbDevice}
     */
    reassignDevice_: {
      type: Object,
      value: null,
    },
  },

  /** @private {settings.PluginVmBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.PluginVmBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'plugin-vm-shared-usb-devices-changed',
        this.onPluginVmSharedUsbDevicesChanged_.bind(this));
    this.browserProxy_.notifyPluginVmSharedUsbDevicesPageReady();
  },

  /**
   * @param {!Array<PluginVmSharedUsbDevice>} devices
   * @private
   */
  onPluginVmSharedUsbDevicesChanged_(devices) {
    this.sharedUsbDevices_ = devices;
  },

  /**
   * @param {!CustomEvent<!PluginVmSharedUsbDevice>} event
   * @private
   */
  onDeviceSharedChange_(event) {
    const device = event.model.item;
    // Show reassign dialog if device is already shared with another VM.
    if (event.target.checked && device.shareWillReassign) {
      event.target.checked = false;
      this.reassignDevice_ = device;
      return;
    }
    this.browserProxy_.setPluginVmUsbDeviceShared(
        device.guid, event.target.checked);
    settings.recordSettingChange();
  },

  /** @private */
  onReassignCancelClick_() {
    this.reassignDevice_ = null;
  },

  /** @private */
  onReassignContinueClick_() {
    this.browserProxy_.setPluginVmUsbDeviceShared(
        this.reassignDevice_.guid, true);
    this.reassignDevice_ = null;
    settings.recordSettingChange();
  },

  /**
   * @param {!PluginVmSharedUsbDevice} device USB device.
   * @private
   */
  getReassignDialogText_(device) {
    return this.i18n('pluginVmSharedUsbDevicesReassign', device.label);
  },
});
