// Copyright 2021 The Chromium Authors
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
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DialogContent, FirmwareUpdate, InstallationProgress, InstallControllerRemote, UpdateProgressObserverInterface, UpdateProgressObserverReceiver, UpdateProviderInterface, UpdateState} from './firmware_update_types.js';
import {getUpdateProvider} from './mojo_interface_provider.js';

/** @type {!Array<!UpdateState>} */
const inactiveDialogStates = [UpdateState.kUnknown, UpdateState.kIdle];

/** @type {!DialogContent} */
const initialDialogContent = {
  title: '',
  body: '',
  footer: '',
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
      /** @type {?FirmwareUpdate} */
      update: {
        type: Object,
      },

      /** @type {!InstallationProgress} */
      installationProgress: {
        type: Object,
        value: {percentage: 0, state: UpdateState.kIdle},
        observer: FirmwareUpdateDialogElement.prototype.progressChanged_,
      },

      /** @private {boolean} */
      isInitiallyInflight_: {
        value: false,
      },

      /** @type {!DialogContent} */
      dialogContent: {
        type: Object,
        value: initialDialogContent,
        computed: 'computeDialogContent_(installationProgress.*,' +
            'isInitiallyInflight_)',
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
      this.isInitiallyInflight_ = e.detail.inflight;
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
    if (update.state === UpdateState.kSuccess ||
        update.state === UpdateState.kFailed) {
      // Install is completed, reset inflight state.
      this.isInitiallyInflight_ = false;
    }
    this.installationProgress = update;
    if (this.isUpdateInProgress_() && this.isDialogOpen_()) {
      // 'aria-hidden' is used to prevent ChromeVox from announcing
      // the body text automatically. Setting 'aria-hidden' to false
      // here allows ChromeVox to announce the body text when a user
      // navigates to it.
      this.shadowRoot.querySelector('#updateDialogBody')
          .setAttribute('aria-hidden', 'false');
    }
  }

  /**
   * @param {!InstallationProgress} prevProgress
   * @param {?InstallationProgress} currProgress
   */
  progressChanged_(prevProgress, currProgress) {
    if (!currProgress || prevProgress.state == currProgress.state) {
      return;
    }
    // Focus the dialog title if the update state has changed.
    const dialogTitle = this.shadowRoot.querySelector('#updateDialogTitle');
    if (dialogTitle) {
      dialogTitle.focus();
    }
  }

  /** @protected */
  closeDialog_() {
    this.isInitiallyInflight_ = false;
    // Resetting |installationProgress| triggers a call to
    // |shouldShowUpdateDialog_|.
    this.installationProgress = {percentage: 0, state: UpdateState.kIdle};
    this.update = null;
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
    this.bindReceiverAndMaybeStartUpdate_();
  }

  /** @protected */
  bindReceiverAndMaybeStartUpdate_() {
    /** @protected {?UpdateProgressObserverReceiver} */
    this.updateProgressObserverReceiver_ = new UpdateProgressObserverReceiver(
        /**
         * @type {!UpdateProgressObserverInterface}
         */
        (this));

    this.installController_.addObserver(
        this.updateProgressObserverReceiver_.$.bindNewPipeAndPassRemote());

    // Only start new updates, inflight updates will be observed instead.
    if (!this.isInitiallyInflight_) {
      this.installController_.beginUpdate(
          this.update.deviceId, this.update.filepath);
    }
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowUpdateDialog_() {
    if (!this.update) {
      return false;
    }

    // Handles the case in which an update is in progress on app load, but has
    // yet to receive an progress update callback.
    if (this.isInitiallyInflight_) {
      return true;
    }

    /** @type {!Array<!UpdateState>} */
    const activeDialogStates = [
      UpdateState.kUpdating,
      UpdateState.kRestarting,
      UpdateState.kFailed,
      UpdateState.kSuccess,
    ];
    // Show dialog is there is an update in progress.
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
  isDeviceRestarting_() {
    return this.installationProgress.state === UpdateState.kRestarting;
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowProgressBar_() {
    const res = this.isUpdateInProgress_() || this.isDeviceRestarting_() ||
        this.isInitiallyInflight_;
    const progressIsActiveEl = this.shadowRoot.activeElement ==
        this.shadowRoot.querySelector('#progress');
    // Move focus to the dialog title if the progress label is currently
    // active and set to be hidden. This case is reached when the dialog state
    // moves from restarting to completed.
    const dialogTitle = this.shadowRoot.querySelector('#updateDialogTitle');
    if (progressIsActiveEl && !res && dialogTitle) {
      dialogTitle.focus();
    }
    return res;
  }
  /**
   * @protected
   * @return {boolean}
   */
  isUpdateDone_() {
    return this.installationProgress.state === UpdateState.kSuccess ||
        this.installationProgress.state === UpdateState.kFailed;
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
      [UpdateState.kRestarting]: {
        title:
            this.i18n('restartingTitleText', mojoString16ToString(deviceName)),
        body: this.i18n('restartingBodyText'),
        footer: this.i18n('restartingFooterText'),
      },
      [UpdateState.kFailed]: {
        title: this.i18n(
            'updateFailedTitleText', mojoString16ToString(deviceName)),
        body: this.i18n('updateFailedBodyText'),
        footer: '',
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
    // No update in progress.
    if (!this.isInitiallyInflight_ && !this.update) {
      return initialDialogContent;
    }

    if (inactiveDialogStates.includes(this.installationProgress.state) ||
        this.isDeviceRestarting_()) {
      return this.createDialogContentObj_(UpdateState.kRestarting);
    }

    // Regular case: Update is in progress, started from the same instance of
    // which the app launched.
    // Edge case: App launch with an update in progress, but no progress
    // callback has been called yet.
    if (this.isInitiallyInflight_ || this.isUpdateInProgress_()) {
      return this.createDialogContentObj_(UpdateState.kUpdating);
    }

    if (this.isUpdateDone_()) {
      return this.createDialogContentObj_(this.installationProgress.state);
    }
    return initialDialogContent;
  }

  /**
   * @protected
   * @return {boolean}
   */
  isInIndeterminateState_() {
    if (this.installationProgress) {
      return inactiveDialogStates.includes(this.installationProgress.state) ||
          this.isDeviceRestarting_();
    }

    return false;
  }

  /**
   * @protected
   * @return {string}
   */
  computeButtonText_() {
    if (!this.isUpdateDone_()) {
      return '';
    }

    return this.installationProgress.state === UpdateState.kSuccess ?
        this.i18n('doneButton') :
        this.i18n('okButton');
  }
  /**
   * @protected
   * @return {boolean}
   */
  isDialogOpen_() {
    return !!this.shadowRoot.querySelector('#updateDialog');
  }
}

customElements.define(
    FirmwareUpdateDialogElement.is, FirmwareUpdateDialogElement);
