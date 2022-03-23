// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'guest-os-shared-usb-devices' is the settings shared usb devices subpage for
 * guest OSes.
 */

import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsSharedUsbDevice} from './guest_os_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-guest-os-shared-usb-devices',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The type of Guest OS to share with. Should be 'crostini' or 'pluginVm'.
     */
    guestOsType: String,

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

  /** @private {GuestOsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = GuestOsBrowserProxyImpl.getInstance();
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
      return {shared: device.sharedWith === this.vmName_(), device: device};
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
        this.vmName_(), device.guid, event.target.checked);
    recordSettingChange();
  },

  /** @private */
  onReassignCancel_() {
    this.reassignDevice_ = null;
  },

  /** @private */
  onReassignContinueClick_() {
    this.browserProxy_.setGuestOsUsbDeviceShared(
        this.vmName_(), this.reassignDevice_.guid, true);
    this.reassignDevice_ = null;
    recordSettingChange();
  },

  /**
   * @private
   * @return {string} The name of the VM to share devices with.
   */
  vmName_() {
    return {
      crostini: 'termina',
      pluginVm: 'PvmDefault',
      arcvm: 'arcvm'
    }[this.guestOsType];
  },

  /**
   * @private
   * @return {string} Description for the page.
   */
  getDescriptionText_() {
    return this.i18n(this.guestOsType + 'SharedUsbDevicesDescription');
  },

  /**
   * @param {!GuestOsSharedUsbDevice} device USB device.
   * @private
   * @return {string} Confirmation prompt text.
   */
  getReassignDialogText_(device) {
    return this.i18n('guestOsSharedUsbDevicesReassign', device.label);
  },
});
