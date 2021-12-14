// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './mojom/firmware_update.mojom-lite.js';
import './update_card.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FirmwareUpdate, UpdateObserverInterface, UpdateObserverReceiver, UpdateProviderInterface} from './firmware_update_types.js';
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
    // Calling observePeripheralUpdates will trigger onUpdateListChanged.
    /** @protected {?UpdateObserverReceiver} */
    this.updateListObserverReceiver_ = new UpdateObserverReceiver(
        /**
         * @type {!UpdateObserverInterface}
         */
        (this));

    this.updateProvider_.observePeripheralUpdates(
        this.updateListObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements UpdateObserver.onUpdateListChanged
   * @param {!Array<!FirmwareUpdate>} firmwareUpdates
   */
  onUpdateListChanged(firmwareUpdates) {
    this.firmwareUpdates_ = firmwareUpdates;
  }

  /**
   * @protected
   * @return {boolean}
   */
  hasFirmwareUpdates_() {
    return this.firmwareUpdates_.length > 0;
  }
}

customElements.define(
    PeripheralUpdateListElement.is, PeripheralUpdateListElement);
