// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './update_card.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FirmwareUpdate, UpdateProviderInterface} from './firmware_update_types.js';
import {getUpdateProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'peripheral-updates-list' displays a list of available peripheral updates.
 */
export class PeripheralUpdateListElement extends PolymerElement {
  static get is() {
    return 'peripheral-updates-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected {!Array<!FirmwareUpdate>} */
      firmwareUpdates_: {
        type: Array,
        value: () => [],
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!UpdateProviderInterface} */
    this.updateProvider_ = getUpdateProvider();
    this.observePeripheralUpdates_();
  }

  /** @private */
  observePeripheralUpdates_() {
    this.updateProvider_.observePeripheralUpdates(this);
  }

  /**
   * Implements DeviceObserver.onUpdateListChanged
   * @param {!Array<!FirmwareUpdate>} firmwareUpdates
   */
  onUpdateListChanged(firmwareUpdates) {
    this.firmwareUpdates_ = firmwareUpdates;
  }
}

customElements.define(
    PeripheralUpdateListElement.is, PeripheralUpdateListElement);
