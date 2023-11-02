// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import '/file_path.mojom-lite.js';
import './mojom/firmware_update.mojom-lite.js';
import './update_card.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FirmwareUpdate, UpdateObserverInterface, UpdateObserverReceiver, UpdateProviderInterface} from './firmware_update_types.js';
import {getUpdateProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'peripheral-updates-list' displays a list of available peripheral updates.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const PeripheralUpdateListElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class PeripheralUpdateListElement extends
    PeripheralUpdateListElementBase {
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

      /** @protected */
      hasCheckedInitialInflightProgress_: {
        type: Boolean,
        value: false,
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
    this.announceNumUpdates_();
    if (!this.hasCheckedInitialInflightProgress_) {
      this.updateProvider_.fetchInProgressUpdate().then(result => {
        if (result.update) {
          this.dispatchEvent(new CustomEvent('open-update-dialog', {
            bubbles: true,
            composed: true,
            detail: {update: result.update, inflight: true},
          }));
        }
        this.hasCheckedInitialInflightProgress_ = true;
      });
    }
  }

  /**
   * @protected
   * @return {boolean}
   */
  hasFirmwareUpdates_() {
    return this.firmwareUpdates_.length > 0;
  }

  /** @protected */
  announceNumUpdates_() {
    IronA11yAnnouncer.requestAvailability();
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {text: this.i18n('numUpdatesText', this.firmwareUpdates_.length)},
    }));
  }
}

customElements.define(
    PeripheralUpdateListElement.is, PeripheralUpdateListElement);
