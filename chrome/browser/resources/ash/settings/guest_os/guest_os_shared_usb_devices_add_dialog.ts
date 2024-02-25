// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-guest-os-shared-usb-devices-add-dialog' is a
 * component enabling a user to add a USB device by filling in the appropriate
 * fields and clicking add.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import './guest_os_container_select.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists, castExists} from '../assert_extras.js';

import {ContainerInfo, GuestId, GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsSharedUsbDevice} from './guest_os_browser_proxy.js';
import {getTemplate} from './guest_os_shared_usb_devices_add_dialog.html.js';

interface GuestOsSharedUsbDevicesAddDialog {
  $: {
    dialog: CrDialogElement,
    selectDevice: HTMLSelectElement,
  };
}

const GuestOsSharedUsbDevicesAddDialogElementBase = I18nMixin(PolymerElement);

class GuestOsSharedUsbDevicesAddDialog extends
    GuestOsSharedUsbDevicesAddDialogElementBase {
  static get is() {
    return 'settings-guest-os-shared-usb-devices-add-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The USB Devices available for connection to a VM.
       */
      sharedUsbDevices: Array,

      defaultGuestId: {
        type: Object,
        value() {
          return {
            vm_name: '',
            container_name: '',
          };
        },
      },

      guestId_: Object,

      /**
       * List of containers that are already stored in the settings.
       */
      allContainers: {
        type: Array,
        value: [],
      },

      /**
       * The USB device which was toggled to be shared, but is already shared
       * with another VM. When non-null the reassign dialog is shown.
       */
      reassignDevice_: {
        type: Object,
        value: null,
      },
    };
  }

  allContainers: ContainerInfo[];
  defaultGuestId: GuestId;
  private browserProxy_: GuestOsBrowserProxy;
  private guestId_: GuestId|null;
  private reassignDevice_: GuestOsSharedUsbDevice|null;
  private sharedUsbDevices:
      Array<{shared: boolean, device: GuestOsSharedUsbDevice}>;

  constructor() {
    super();

    this.browserProxy_ = GuestOsBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.dialog.showModal();
    microTask.run(() => {
      this.$.selectDevice.focus();
    });
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onAddClick_(): void {
    const sharedUsbDevice = this.sharedUsbDevices.find(
        ({device}) => device.guid === this.$.selectDevice.value);
    const {device} = castExists(sharedUsbDevice);
    if (device.promptBeforeSharing) {
      this.reassignDevice_ = device;
      return;
    }

    const guestId = this.guestId_ || this.defaultGuestId;
    this.browserProxy_.setGuestOsUsbDeviceShared(
        guestId.vm_name, guestId.container_name, device.guid, true);
    this.$.dialog.close();
  }

  private onReassignCancel_(): void {
    this.reassignDevice_ = null;
  }

  private onReassignContinueClick_(): void {
    assertExists(this.reassignDevice_);
    const guestId = this.guestId_ || this.defaultGuestId;
    this.browserProxy_.setGuestOsUsbDeviceShared(
        guestId.vm_name, guestId.container_name, this.reassignDevice_.guid,
        true);
    this.reassignDevice_ = null;
    this.$.dialog.close();
  }

  /**
   * @param device USB device.
   * @return Confirmation prompt text.
   */
  private getReassignDialogText_(device: GuestOsSharedUsbDevice): string {
    return this.i18n('guestOsSharedUsbDevicesReassign', device.label);
  }

  private showContainerSelect_(allContainers: ContainerInfo[]): boolean {
    return allContainers.length > 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-guest-os-shared-usb-devices-add-dialog':
        GuestOsSharedUsbDevicesAddDialog;
  }
}

customElements.define(
    GuestOsSharedUsbDevicesAddDialog.is, GuestOsSharedUsbDevicesAddDialog);
