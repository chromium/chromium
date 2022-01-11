// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './mojom/firmware_update.mojom-lite.js';
import './strings.m.js';

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FirmwareUpdate, InstallationProgress, InstallControllerRemote, UpdateProgressObserverInterface, UpdateProgressObserverReceiver, UpdateProviderInterface, UpdateState} from './firmware_update_types.js';
import {getUpdateProvider} from './mojo_interface_provider.js';
import {mojoString16ToString} from './mojo_utils.js';

/** @enum {number} */
export const DialogState = {
  CLOSED: 0,
  UPDATING: 2,
  UPDATE_DONE: 3,
};

/**
 * @fileoverview
 * 'firmware-update-dialog' displays information related to a firmware update.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const FirmwareUpdateDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class FirmwareUpdateDialogElement extends
    FirmwareUpdateDialogElementBase {
  static get is() {
    return 'firmware-update-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!FirmwareUpdate} */
      update: {
        type: Object,
      },

      /** @type {?InstallationProgress} */
      installationProgress: {
        type: Object,
      },

      /** @type {!DialogState} */
      dialogState: {
        type: Number,
        value: DialogState.CLOSED,
        computed: 'onStateChanged_(installationProgress.state)'
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!UpdateProviderInterface} */
    this.updateProvider_ = getUpdateProvider();

    /** @type {?InstallControllerRemote} */
    this.installController_ = null;

    /**
     * Event callback for 'open-update-dialog'.
     * @param {!Event} e
     * @private
     */
    this.openUpdateDialog_ = (e) => {
      this.update = e.detail.update;
      this.prepareForUpdate_();
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    window.addEventListener(
        'open-update-dialog', (e) => this.openUpdateDialog_(e));
  }

  /**
   * Implements UpdateProgressObserver.onStatusChanged
   * @param {!InstallationProgress} update
   */
  onStatusChanged(update) {
    this.installationProgress = update;
  }

  /** @protected */
  onStateChanged_() {
    if (!this.installationProgress) {
      return DialogState.CLOSED;
    }
    // TODO(michaelcheco): Handle restarting and failed states.
    switch (this.installationProgress.state) {
      case UpdateState.kUnknown:
      case UpdateState.kIdle:
        return DialogState.CLOSED;
      case UpdateState.kUpdating:
      case UpdateState.kRestarting:
        return DialogState.UPDATING;
      case UpdateState.kFailed:
      case UpdateState.kSuccess:
        return DialogState.UPDATE_DONE;
    }
  }

  /** @protected */
  closeDialog_() {
    this.dialogState = DialogState.CLOSED;
    this.installationProgress = null;
  }

  /** @protected */
  async prepareForUpdate_() {
    const response =
        await this.updateProvider_.prepareForUpdate(this.update.deviceId);
    if (!response.controller) {
      // TODO(michaelcheco): Handle |StartInstall| failed case.
      return;
    }
    this.installController_ =
        /**@type {InstallControllerRemote} */ (response.controller);
    this.beginUpdate_();
  }

  /** @protected */
  beginUpdate_() {
    /** @protected {?UpdateProgressObserverReceiver} */
    this.updateProgressObserverReceiver_ = new UpdateProgressObserverReceiver(
        /**
         * @type {!UpdateProgressObserverInterface}
         */
        (this));

    this.installController_.addObserver(
        this.updateProgressObserverReceiver_.$.bindNewPipeAndPassRemote());
    this.installController_.beginUpdate();
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowUpdateDialog_() {
    return this.isUpdateInProgress_() ||
        this.dialogState === DialogState.UPDATE_DONE;
  }

  /**
   * @protected
   * @return {number}
   */
  computePercentageValue_() {
    if (this.installationProgress && this.installationProgress.percentage) {
      return this.installationProgress.percentage;
    }
    return 0;
  }

  /**
   * @protected
   * @return {boolean}
   */
  isUpdateInProgress_() {
    return this.dialogState === DialogState.UPDATING;
  }

  /**
   * @protected
   * @return {string}
   */
  computeUpdateDialogTitle_() {
    return this.isUpdateInProgress_() ?
        this.i18n('updating', mojoString16ToString(this.update.deviceName)) :
        this.i18n(
            'deviceUpToDate', mojoString16ToString(this.update.deviceName));
  }

  /**
   * @protected
   * @return {string}
   */
  computeProgressText_() {
    return this.i18n('installing', this.installationProgress.percentage);
  }

  /**
   * @protected
   * @return {string}
   */
  computeUpdateDialogBodyText_() {
    const {deviceName, deviceVersion} = this.update;
    return this.dialogState === DialogState.UPDATE_DONE ?
        this.i18n(
            'hasBeenUpdated', mojoString16ToString(deviceName), deviceVersion) :
        this.i18n('updatingInfo');
  }
}

customElements.define(
    FirmwareUpdateDialogElement.is, FirmwareUpdateDialogElement);
