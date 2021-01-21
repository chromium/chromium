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

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The USB Devices available for connection to a VM.
     * @private {Array<{shared: boolean, device: !GuestOsSharedUsbDevice}>}
     */
    sharedUsbDevices_: Array,

    /**
     * The USB device which was toggled to be shared, but is already shared
     * with another VM. When non-null the reassign dialog is shown.
     * @private {?GuestOsSharedUsbDevice}
     */
    reassignDevice_: {
      type: Object,
      value: null,
    },
  },

  /**
   * The default crostini VM is named 'termina'.
   * https://cs.chromium.org/chromium/src/chrome/browser/chromeos/crostini/crostini_util.h?q=kCrostiniDefaultVmName&dr=CSs
   * @private {string}
   */
  vmName_: 'termina',

  /** @private {settings.GuestOsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.GuestOsBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'guest-os-shared-usb-devices-changed',
        this.onGuestOsSharedUsbDevicesChanged_.bind(this));
    this.browserProxy_.notifyGuestOsSharedUsbDevicesPageReady();
  },

  /**
   * @param {!Array<GuestOsSharedUsbDevice>} devices
   * @private
   */
  onGuestOsSharedUsbDevicesChanged_(devices) {
    this.sharedUsbDevices_ = devices.map((device) => {
      return {shared: device.sharedWith === this.vmName_, device: device};
    });
  },

  /**
   * @param {!CustomEvent<!GuestOsSharedUsbDevice>} event
   * @private
   */
  onDeviceSharedChange_(event) {
    const device = event.model.item.device;
    // Show reassign dialog if device is already shared with another VM.
    if (event.target.checked && device.promptBeforeSharing) {
      event.target.checked = false;
      this.reassignDevice_ = device;
      return;
    }
    this.browserProxy_.setGuestOsUsbDeviceShared(
        this.vmName_, device.guid, event.target.checked);
    settings.recordSettingChange();
  },

  /** @private */
  onReassignCancel_() {
    this.reassignDevice_ = null;
  },

  /** @private */
  onReassignContinueClick_() {
    this.browserProxy_.setGuestOsUsbDeviceShared(
        this.vmName_, this.reassignDevice_.guid, true);
    this.reassignDevice_ = null;
    settings.recordSettingChange();
  },

  /**
   * @param {!GuestOsSharedUsbDevice} device USB device.
   * @private
   */
  getReassignDialogText_(device) {
    return this.i18n('crostiniSharedUsbDevicesReassign', device.label);
  },
});
