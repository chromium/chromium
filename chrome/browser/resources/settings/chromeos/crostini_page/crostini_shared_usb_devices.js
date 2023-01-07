// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-shared-usb-devices' is a variant of the
 * shared usb devices subpage for Crostini.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {GuestId} from '../guest_os/guest_os_browser_proxy.js';
import {SettingsGuestOsSharedUsbDevicesElement} from '../guest_os/guest_os_shared_usb_devices.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CROSTINI_GUEST_ID} from './crostini_browser_proxy.js';

/** @polymer */
class CrostiniSharedUsbDevicesElement extends
    SettingsGuestOsSharedUsbDevicesElement {
  static get is() {
    return 'settings-crostini-shared-usb-devices';
  }

  static get properties() {
    return {
      guestOsType: {
        type: String,
        value: 'crostini',
      },

      /**
       * @type {!GuestId}
       */
      defaultGuestId: {
        type: Object,
        value() {
          return DEFAULT_CROSTINI_GUEST_ID;
        },
      },

      /**
       * Whether the guest OS hosts multiple containers.
       */
      hasContainers: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showCrostiniExtraContainers');
        },
      },
    };
  }

  constructor() {
    super();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'crostini-container-info', (infos) => this.onContainerInfo_(infos));
    CrostiniBrowserProxyImpl.getInstance().requestContainerInfo();
  }
}

customElements.define(
    CrostiniSharedUsbDevicesElement.is, CrostiniSharedUsbDevicesElement);
