// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import '/file_path.mojom-lite.js';
import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './mojom/firmware_update.mojom-lite.js';
import './strings.m.js';

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {DialogContent, FirmwareUpdate, InstallationProgress, InstallControllerRemote, UpdateProgressObserverInterface, UpdateProgressObserverReceiver, UpdateProviderInterface, UpdateState} from './firmware_update_types.js';
import {getUpdateProvider} from './mojo_interface_provider.js';
import {mojoString16ToString} from './mojo_utils.js';

/** @type {!DialogContent} */
const initialDialogContent = {
  title: '',
  body: '',
  footer: ''
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

      /** @type {!InstallationProgress} */
      installationProgress: {
        type: Object,
      },

      /** @type {!DialogContent} */
      dialogContent: {
        type: Object,
        value: initialDialogContent,
        computed: 'computeDialogContent_(installationProgress.*)',
      }
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
  closeDialog_() {
    // Resetting |installationProgress| triggers a call to
    // |shouldShowUpdateDialog_|.
    this.installationProgress = {percentage: 0, state: UpdateState.kIdle};
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
    /** @type {!Array<!UpdateState>} */
    const activeDialogStates = [
      UpdateState.kUpdating,
      UpdateState.kRestarting,
      UpdateState.kFailed,
      UpdateState.kSuccess,
    ];
    return activeDialogStates.includes(this.installationProgress.state) ||
        this.installationProgress.percentage > 0;
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
    /** @type {!Array<!UpdateState>} */
    const inactiveDialogStates = [UpdateState.kUnknown, UpdateState.kIdle];
    if (inactiveDialogStates.includes(this.installationProgress.state)) {
      return this.installationProgress.percentage > 0;
    }

    return this.installationProgress.state === UpdateState.kUpdating;
  }

  /**
   * @protected
   * @return {boolean}
   */
  isUpdateDone_() {
    // TODO(michaelcheco): Handle failed state.
    return this.installationProgress.state === UpdateState.kSuccess;
  }

  /**
   * @param {!UpdateState} state
   * @return {!DialogContent}
   */
  createDialogContentObj_(state) {
    const {deviceName, deviceVersion} = this.update;
    const {percentage} = this.installationProgress;

    const dialogContent = {
      [UpdateState.kUpdating]: {
        title: this.i18n('updating', mojoString16ToString(deviceName)),
        body: this.i18n('updatingInfo'),
        footer: this.i18n('installing', percentage),
      },
      [UpdateState.kSuccess]: {
        title: this.i18n('deviceUpToDate', mojoString16ToString(deviceName)),
        body: this.i18n(
            'hasBeenUpdated', mojoString16ToString(deviceName), deviceVersion),
        footer: '',
      },
    };

    return dialogContent[state];
  }

  /** @return {!DialogContent} */
  computeDialogContent_() {
    if (this.isUpdateInProgress_()) {
      return this.createDialogContentObj_(UpdateState.kUpdating);
    }

    if (this.isUpdateDone_()) {
      return this.createDialogContentObj_(UpdateState.kSuccess);
    }
    return initialDialogContent;
  }
}

customElements.define(
    FirmwareUpdateDialogElement.is, FirmwareUpdateDialogElement);
