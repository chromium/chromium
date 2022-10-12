// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'guest-os-shared-usb-devices' is the settings shared usb devices subpage for
 * guest OSes.
 */

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import './guest_os_shared_usb_devices_add_dialog.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {ContainerInfo, GuestId, GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsSharedUsbDevice} from './guest_os_browser_proxy.js';
import {containerLabel, equalContainerId} from './guest_os_container_select.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsGuestOsSharedUsbDevicesElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
export class SettingsGuestOsSharedUsbDevicesElement extends
    SettingsGuestOsSharedUsbDevicesElementBase {
  static get is() {
    return 'settings-guest-os-shared-usb-devices';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The type of Guest OS to share with. Should be 'crostini' or 'pluginVm'.
       */
      guestOsType: String,

      /**
       * The USB Devices available for connection to a VM.
       * @private {!Array<{shared: boolean, device: !GuestOsSharedUsbDevice}>}
       */
      sharedUsbDevices_: {
        type: Array,
        value() {
          return [];
        },
      },

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
       * The USB device which was toggled to be shared, but is already shared
       * with another VM. When non-null the reassign dialog is shown.
       * @private {?GuestOsSharedUsbDevice}
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
        value: false,
      },

      /** @private */
      showAddUsbDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * The known ContainerIds for display in the UI.
       * @private {!Array<!ContainerInfo>}
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

  constructor() {
    super();

    /** @private {!GuestOsBrowserProxy} */
    this.browserProxy_ = GuestOsBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'guest-os-shared-usb-devices-changed',
        this.onGuestOsSharedUsbDevicesChanged_.bind(this));
    this.browserProxy_.notifyGuestOsSharedUsbDevicesPageReady();
  }

  /**
   * @param {!Array<!ContainerInfo>} containerInfos
   * @protected
   */
  onContainerInfo_(containerInfos) {
    this.set('allContainers_', containerInfos);
  }

  /**
   * @param {!Array<{shared: boolean, device: !GuestOsSharedUsbDevice}>}
   *     sharedUsbDevices
   * @param {!GuestId} id
   * @return boolean
   * @private
   */
  showGuestId_(sharedUsbDevices, id) {
    return sharedUsbDevices.some(this.byGuestId_(id));
  }

  /**
   * @param {!Array<{shared: boolean, device: !GuestOsSharedUsbDevice}>}
   *     sharedUsbDevices
   * @param {!Array<!ContainerInfo>} containerInfos
   * @return boolean
   * @private
   */
  hasSharedDevices_(sharedUsbDevices, containerInfos) {
    return sharedUsbDevices.some(
        dev => containerInfos.some(
            info => dev.device.guestId &&
                equalContainerId(dev.device.guestId, info.id)));
  }

  /**
   * @param {!Array<GuestOsSharedUsbDevice>} devices
   * @private
   */
  onGuestOsSharedUsbDevicesChanged_(devices) {
    this.sharedUsbDevices_ = devices.map((device) => {
      return {
        shared: device.guestId && device.guestId.vm_name === this.vmName_(),
        device: device,
      };
    });
  }

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
        this.vmName_(), this.defaultGuestId.container_name, device.guid,
        event.target.checked);
    recordSettingChange();
  }

  /** @private */
  onReassignCancel_() {
    this.reassignDevice_ = null;
  }

  /** @private */
  onReassignContinueClick_() {
    this.browserProxy_.setGuestOsUsbDeviceShared(
        this.vmName_(), this.defaultGuestId.container_name,
        this.reassignDevice_.guid, true);
    this.reassignDevice_ = null;
    recordSettingChange();
  }

  /**
   * @private
   * @return {string} The name of the VM to share devices with.
   */
  vmName_() {
    return {
      crostini: 'termina',
      pluginVm: 'PvmDefault',
      arcvm: 'arcvm',
      bruschetta: 'bru',
    }[this.guestOsType];
  }

  /**
   * @private
   * @return {string} Description for the page.
   */
  getDescriptionText_() {
    return this.i18n(this.guestOsType + 'SharedUsbDevicesDescription');
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
   * @param {!GuestId} id
   * @return function({shared: boolean, device: !GuestOsSharedUsbDevice}):
   *     boolean
   * @private
   */
  byGuestId_(id) {
    return dev =>
               dev.device.guestId && equalContainerId(dev.device.guestId, id);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onAddUsbClick_(event) {
    this.showAddUsbDialog_ = true;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onAddUsbDialogClose_(event) {
    this.showAddUsbDialog_ = false;
  }

  /**
   * @param {!GuestId} id
   * @return string
   * @private
   */
  guestLabel_(id) {
    return containerLabel(id, this.vmName_());
  }

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveUsbClick_(event) {
    const device = event.model.item.device;
    if (device.guestId != null) {
      this.browserProxy_.setGuestOsUsbDeviceShared(
          device.guestId.vm_name, '', device.guid, false);
    }
  }
}

customElements.define(
    SettingsGuestOsSharedUsbDevicesElement.is,
    SettingsGuestOsSharedUsbDevicesElement);
