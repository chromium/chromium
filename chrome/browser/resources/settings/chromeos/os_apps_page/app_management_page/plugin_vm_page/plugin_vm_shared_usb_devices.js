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

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * The USB Devices available for connection to a VM.
     * @private {Array<!PluginVmSharedUsbDevice>}
     */
    sharedUsbDevices_: Array,
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
    const deviceInfo = event.model.item;
    this.browserProxy_.setPluginVmUsbDeviceShared(
        deviceInfo.guid, event.target.checked);
    settings.recordSettingChange();
  },
});
