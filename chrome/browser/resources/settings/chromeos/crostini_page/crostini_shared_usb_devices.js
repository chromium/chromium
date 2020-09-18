// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-shared-usb-devices' is the settings shared usb devices subpage for
 * Crostini.
 */

Polymer({
  is: 'settings-crostini-shared-usb-devices',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * The USB Devices available for connection to a VM.
     * @private {Array<!CrostiniSharedUsbDevice>}
     */
    sharedUsbDevices_: Array,
  },

  /** @private {settings.CrostiniBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.CrostiniBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'crostini-shared-usb-devices-changed',
        this.onCrostiniSharedUsbDevicesChanged_.bind(this));
    this.browserProxy_.notifyCrostiniSharedUsbDevicesPageReady();
  },

  /**
   * @param {!Array<CrostiniSharedUsbDevice>} devices
   * @private
   */
  onCrostiniSharedUsbDevicesChanged_(devices) {
    this.sharedUsbDevices_ = devices;
  },

  /**
   * @param {!CustomEvent<!CrostiniSharedUsbDevice>} event
   * @private
   */
  onDeviceSharedChange_(event) {
    const deviceInfo = event.model.item;
    this.browserProxy_.setCrostiniUsbDeviceShared(
        deviceInfo.guid, event.target.checked);
    settings.recordSettingChange();
  },
});
