// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'guest-os-shared-usb-devices' is the settings shared usb devices subpage for
 * guest OSes.
 */

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import './guest_os_shared_usb_devices_add_dialog.js';

import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists, cast} from '../assert_extras.js';
import {recordSettingChange} from '../metrics_recorder.js';

import {ContainerInfo, getVMNameForGuestOsType, GuestId, GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsSharedUsbDevice, GuestOsType} from './guest_os_browser_proxy.js';
import {containerLabel, equalContainerId} from './guest_os_container_select.js';
import {getTemplate} from './guest_os_shared_usb_devices.html.js';

interface SharedUsbDevice {
  shared: boolean;
  device: GuestOsSharedUsbDevice;
}

const SettingsGuestOsSharedUsbDevicesElementBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

export class SettingsGuestOsSharedUsbDevicesElement extends
    SettingsGuestOsSharedUsbDevicesElementBase {
  static get is() {
    return 'settings-guest-os-shared-usb-devices';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The type of Guest OS to share with. Should be 'crostini' or 'pluginVm'.
       */
      guestOsType: {
        type: String,
        value: '',
      },

      /**
       * The USB Devices available for connection to a VM.
       */
      sharedUsbDevices_: {
        type: Array,
        value() {
          return [];
        },
      },

      defaultGuestId: {
        type: Object,
        value() {
          return {
            vm_name: '',
            container_name: '',
          };
        },
      },

      /**
       * The USB device which was toggled to be shared, but is already shared
       * with another VM. When non-null the reassign dialog is shown.
       */
      reassignDevice_: {
        type: Object,
        value: null,
      },

      /**
       * Whether the guest OS hosts multiple containers.
       */
      hasContainers: {
        type: Boolean,
        value() {
          return false;
        },
      },

      showAddUsbDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * The known ContainerIds for display in the UI.
       */
      allContainers_: {
        type: Array,
        notify: true,
        value() {
          return [];
        },
      },
    };
  }

  defaultGuestId: GuestId;
  guestOsType: GuestOsType;
  hasContainers: boolean;
  private allContainers_: ContainerInfo[];
  private browserProxy_: GuestOsBrowserProxy;
  private reassignDevice_: GuestOsSharedUsbDevice|null;
  private sharedUsbDevices_: SharedUsbDevice[];
  private showAddUsbDialog_: boolean;

  constructor() {
    super();

    this.browserProxy_ = GuestOsBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'guest-os-shared-usb-devices-changed',
        this.onGuestOsSharedUsbDevicesChanged_.bind(this));
    this.browserProxy_.notifyGuestOsSharedUsbDevicesPageReady();
  }

  protected onContainerInfo_(containerInfos: ContainerInfo[]): void {
    this.set('allContainers_', containerInfos);
  }

  private showGuestId_(sharedUsbDevices: SharedUsbDevice[], id: GuestId):
      boolean {
    return sharedUsbDevices.some(this.byGuestId_(id));
  }

  private hasSharedDevices_(
      sharedUsbDevices: SharedUsbDevice[],
      containerInfos: ContainerInfo[]): boolean {
    return sharedUsbDevices.some(
        dev => containerInfos.some(
            info => dev.device.guestId &&
                equalContainerId(dev.device.guestId, info.id)));
  }

  private onGuestOsSharedUsbDevicesChanged_(devices: GuestOsSharedUsbDevice[]):
      void {
    this.sharedUsbDevices_ = devices.map((device) => {
      return {
        shared: !!device.guestId && device.guestId.vm_name === this.vmName_(),
        device: device,
      };
    });
  }

  private onDeviceSharedChange_(event: DomRepeatEvent<SharedUsbDevice>): void {
    const device = event.model.item.device;
    // Show reassign dialog if device is already shared with another VM.
    const target = cast(event.target, CrToggleElement);
    if (target.checked && device.promptBeforeSharing) {
      target.checked = false;
      this.reassignDevice_ = device;
      return;
    }
    this.browserProxy_.setGuestOsUsbDeviceShared(
        this.vmName_(), this.defaultGuestId.container_name, device.guid,
        target.checked);
    recordSettingChange();
  }

  private onReassignCancel_(): void {
    this.reassignDevice_ = null;
  }

  private onReassignContinueClick_(): void {
    assertExists(this.reassignDevice_);
    this.browserProxy_.setGuestOsUsbDeviceShared(
        this.vmName_(), this.defaultGuestId.container_name,
        this.reassignDevice_.guid, true);
    this.reassignDevice_ = null;
    recordSettingChange();
  }

  private vmName_(): string {
    return getVMNameForGuestOsType(this.guestOsType);
  }

  private getDescriptionText_(): string {
    return this.i18n(this.guestOsType + 'SharedUsbDevicesDescription');
  }

  private getReassignDialogText_(device: GuestOsSharedUsbDevice): string {
    return this.i18n('guestOsSharedUsbDevicesReassign', device.label);
  }

  private byGuestId_(id: GuestId): (device: SharedUsbDevice) => boolean {
    return (dev: SharedUsbDevice) =>
               (!!dev.device.guestId &&
                equalContainerId(dev.device.guestId, id));
  }

  private onAddUsbClick_(): void {
    this.showAddUsbDialog_ = true;
  }

  private onAddUsbDialogClose_(): void {
    this.showAddUsbDialog_ = false;
  }

  private guestLabel_(id: GuestId): string {
    return containerLabel(id, this.vmName_());
  }

  private onRemoveUsbClick_(event: DomRepeatEvent<SharedUsbDevice>): void {
    const device = event.model.item.device;
    if (device.guestId) {
      this.browserProxy_.setGuestOsUsbDeviceShared(
          device.guestId.vm_name, '', device.guid, false);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-guest-os-shared-usb-devices':
        SettingsGuestOsSharedUsbDevicesElement;
  }
}

customElements.define(
    SettingsGuestOsSharedUsbDevicesElement.is,
    SettingsGuestOsSharedUsbDevicesElement);
