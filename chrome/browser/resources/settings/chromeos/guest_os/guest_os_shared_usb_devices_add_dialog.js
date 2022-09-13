// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-guest-os-shared-usb-devices-add-dialog' is a
 * component enabling a user to add a USB device by filling in the appropriate
 * fields and clicking add.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import './guest_os_container_select.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {html, microTask, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {ContainerInfo, GuestId, GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsSharedUsbDevice} from './guest_os_browser_proxy.js';
import {containerLabel} from './guest_os_container_select.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const GuestOsSharedUsbDevicesAddDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class GuestOsSharedUsbDevicesAddDialog extends
    GuestOsSharedUsbDevicesAddDialogElementBase {
  static get is() {
    return 'settings-guest-os-shared-usb-devices-add-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The USB Devices available for connection to a VM.
       * @private {!Array<{shared: boolean, device: !GuestOsSharedUsbDevice}>}
       */
      sharedUsbDevices: Array,

      /**
       * @type {!GuestId}
       */
      defaultGuestId: {
        type: Object,
        value() {
          return {
            'vm_name': '',
            'container_name': '',
          };
        },
      },

      /**
       * @type {GuestId}
       */
      guestId_: Object,

      /**
       * List of containers that are already stored in the settings.
       * @type {!Array<!ContainerInfo>}
       */
      allContainers: {
        type: Array,
        value: [],
      },

      /**
       * The USB device which was toggled to be shared, but is already shared
       * with another VM. When non-null the reassign dialog is shown.
       * @private {?GuestOsSharedUsbDevice}
       */
      reassignDevice_: {
        type: Object,
        value: null,
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
    microTask.run(() => {
      this.$.selectDevice.focus();
    });
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  }

  /** @private */
  onAddTap_() {
    const sharedUsbDevice = this.sharedUsbDevices.find(
        ({device}) => device.guid === this.$.selectDevice.value);
    assert(sharedUsbDevice);

    const {device} = sharedUsbDevice;
    if (device.promptBeforeSharing) {
      this.reassignDevice_ = device;
      return;
    }

    const guestId = this.guestId_ || this.defaultGuestId;
    GuestOsBrowserProxyImpl.getInstance().setGuestOsUsbDeviceShared(
        guestId.vm_name, guestId.container_name, device.guid, true);
    this.$.dialog.close();
    recordSettingChange();
  }

  /** @private */
  onReassignCancel_() {
    this.reassignDevice_ = null;
  }

  /** @private */
  onReassignContinueClick_() {
    const guestId = this.guestId_ || this.defaultGuestId;
    GuestOsBrowserProxyImpl.getInstance().setGuestOsUsbDeviceShared(
        guestId.vm_name, guestId.container_name, this.reassignDevice_.guid,
        true);
    this.reassignDevice_ = null;
    this.$.dialog.close();
    recordSettingChange();
  }

  /**
   * @param {!GuestOsSharedUsbDevice} device USB device.
   * @private
   * @return {string} Confirmation prompt text.
   */
  getReassignDialogText_(device) {
    return this.i18n('guestOsSharedUsbDevicesReassign', device.label);
  }

  /**
   * @param {!Array<!ContainerInfo>} allContainers
   * @return boolean
   * @private
   */
  showContainerSelect_(allContainers) {
    return allContainers.length > 1;
  }
}

customElements.define(
    GuestOsSharedUsbDevicesAddDialog.is, GuestOsSharedUsbDevicesAddDialog);
