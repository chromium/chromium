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

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'crostini-shared-usb-devices-changed',
        this.onCrostiniSharedUsbDevicesChanged_.bind(this));
    settings.CrostiniBrowserProxyImpl.getInstance()
        .getCrostiniSharedUsbDevices()
        .then(this.onCrostiniSharedUsbDevicesChanged_.bind(this));
  },

  /**
   * @param {!Array<CrostiniSharedUsbDevice>} devices
   * @private
   */
  onCrostiniSharedUsbDevicesChanged_: function(devices) {
    this.sharedUsbDevices_ = devices;
  },

  /**
   * @param {!CustomEvent<!CrostiniSharedUsbDevice>} event
   * @private
   */
  onDeviceSharedChange_: function(event) {
    const deviceInfo = event.model.item;
    settings.CrostiniBrowserProxyImpl.getInstance().setCrostiniUsbDeviceShared(
        deviceInfo.guid, event.target.checked);
  },
});
