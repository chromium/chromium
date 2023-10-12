// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './firmware_shared.css.js';
import './firmware_shared_fonts.css.js';
import './firmware_update.mojom-webui.js';
import './strings.m.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FirmwareUpdate, InstallationProgress, InstallControllerRemote, UpdateProgressObserverInterface, UpdateProgressObserverReceiver, UpdateState} from './firmware_update.mojom-webui.js';
import {getTemplate} from './firmware_update_dialog.html.js';
import {DialogContent, OpenUpdateDialogEventDetail} from './firmware_update_types.js';
import {getUpdateProvider} from './mojo_interface_provider.js';

const inactiveDialogStates: UpdateState[] =
    [UpdateState.kUnknown, UpdateState.kIdle];

const initialDialogContent: DialogContent = {
  title: '',
  body: '',
  footer: '',
};

const initialInstallationProgress: InstallationProgress = {
  percentage: 0,
  state: UpdateState.kIdle,
};

/**
 * @fileoverview
 * 'firmware-update-dialog' displays information related to a firmware update.
 */

const FirmwareUpdateDialogElementBase =
    I18nMixin(PolymerElement) as {new (): PolymerElement & I18nMixinInterface};

export class FirmwareUpdateDialogElement extends FirmwareUpdateDialogElementBase
    implements UpdateProgressObserverInterface {
  static get is() {
    return 'firmware-update-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      update: {
        type: Object,
      },

      installationProgress: {
        type: Object,
        value: initialInstallationProgress,
        observer:
            FirmwareUpdateDialogElement.prototype.installationProgressChanged,
      },

      isInitiallyInflight: {
        value: false,
      },

      dialogContent: {
        type: Object,
        value: initialDialogContent,
        computed: 'computeDialogContent(installationProgress.*,' +
            'isInitiallyInflight)',
      },
    };
  }

  update: FirmwareUpdate|null = null;
  installationProgress: InstallationProgress;
  private isInitiallyInflight = false;
  dialogContent = initialDialogContent;
  private updateProvider = getUpdateProvider();
  private installController: InstallControllerRemote|null = null;
  private updateProgressObserverReceiver: UpdateProgressObserverReceiver|null =
      null;

  override connectedCallback() {
    super.connectedCallback();

    window.addEventListener(
        'open-update-dialog',
        (e) => this.onOpenUpdateDialog(
            e as CustomEvent<OpenUpdateDialogEventDetail>));
  }

  /** Implements UpdateProgressObserver.onStatusChanged */
  onStatusChanged(update: InstallationProgress): void {
    if (update.state === UpdateState.kSuccess ||
        update.state === UpdateState.kFailed) {
      // Install is completed, reset inflight state.
      this.isInitiallyInflight = false;
    }
    this.installationProgress = update;
    if (this.isUpdateInProgress() && this.isDialogOpen()) {
      // 'aria-hidden' is used to prevent ChromeVox from announcing
      // the body text automatically. Setting 'aria-hidden' to false
      // here allows ChromeVox to announce the body text when a user
      // navigates to it.
      assert(this.shadowRoot);
      this.shadowRoot.querySelector('#updateDialogBody')!.setAttribute(
          'aria-hidden', 'false');
    }
  }

  protected installationProgressChanged(
      prevProgress: FirmwareUpdateDialogElement['installationProgress'],
      currProgress: FirmwareUpdateDialogElement['installationProgress']): void {
    if (!currProgress || prevProgress.state == currProgress.state) {
      return;
    }
    // Focus the dialog title if the update state has changed.
    assert(this.shadowRoot);
    const dialogTitle =
        this.shadowRoot.querySelector<HTMLElement>('#updateDialogTitle');
    if (dialogTitle) {
      dialogTitle.focus();
    }
  }

  protected closeDialog(): void {
    this.isInitiallyInflight = false;
    // Resetting |installationProgress| triggers a call to
    // |shouldShowUpdateDialog|.
    this.installationProgress = initialInstallationProgress;
    this.update = null;
  }

  protected async prepareForUpdate(): Promise<void> {
    assert(this.update);
    const response =
        await this.updateProvider.prepareForUpdate(this.update.deviceId);
    if (!response.controller) {
      // TODO(michaelcheco): Handle |StartInstall| failed case.
      return;
    }
    this.installController = response.controller;
    this.bindReceiverAndMaybeStartUpdate();
  }

  protected bindReceiverAndMaybeStartUpdate(): void {
    this.updateProgressObserverReceiver =
        new UpdateProgressObserverReceiver(this);
    assert(this.installController);
    this.installController.addObserver(
        this.updateProgressObserverReceiver.$.bindNewPipeAndPassRemote());

    // Only start new updates, inflight updates will be observed instead.
    if (!this.isInitiallyInflight) {
      assert(this.update);
      this.installController.beginUpdate(
          this.update.deviceId, this.update.filepath);
    }
  }

  protected shouldShowUpdateDialog(): boolean {
    if (!this.update) {
      return false;
    }

    // Handles the case in which an update is in progress on app load, but has
    // yet to receive an progress update callback.
    if (this.isInitiallyInflight) {
      return true;
    }

    const activeDialogStates: UpdateState[] = [
      UpdateState.kUpdating,
      UpdateState.kRestarting,
      UpdateState.kFailed,
      UpdateState.kSuccess,
    ];
    // Show dialog is there is an update in progress.
    return activeDialogStates.includes(this.installationProgress.state) ||
        this.installationProgress.percentage > 0;
  }

  protected computePercentageValue(): number {
    if (this.installationProgress?.percentage) {
      return this.installationProgress.percentage;
    }
    return 0;
  }

  protected isUpdateInProgress(): boolean {
    const inactiveDialogStates: UpdateState[] =
        [UpdateState.kUnknown, UpdateState.kIdle];
    if (inactiveDialogStates.includes(this.installationProgress.state)) {
      return this.installationProgress.percentage > 0;
    }

    return this.installationProgress.state === UpdateState.kUpdating;
  }

  protected isDeviceRestarting(): boolean {
    return this.installationProgress.state === UpdateState.kRestarting;
  }

  protected shouldShowProgressBar(): boolean {
    const res = this.isUpdateInProgress() || this.isDeviceRestarting() ||
        this.isInitiallyInflight;
    assert(this.shadowRoot);
    const progressIsActiveEl = this.shadowRoot.activeElement ==
        this.shadowRoot.querySelector('#progress');
    // Move focus to the dialog title if the progress label is currently
    // active and set to be hidden. This case is reached when the dialog state
    // moves from restarting to completed.
    const dialogTitle =
        this.shadowRoot.querySelector<HTMLElement>('#updateDialogTitle');
    if (progressIsActiveEl && !res && dialogTitle) {
      dialogTitle.focus();
    }
    return res;
  }

  protected isUpdateDone(): boolean {
    return this.installationProgress.state === UpdateState.kSuccess ||
        this.installationProgress.state === UpdateState.kFailed;
  }

  createDialogContentObj(state: UpdateState): DialogContent {
    assert(this.update);
    const {deviceName, deviceVersion} = this.update;
    const {percentage} = this.installationProgress;

    const dialogContent = new Map<UpdateState, DialogContent>([
      [
        UpdateState.kUpdating,
        {
          title: this.i18n('updating', mojoString16ToString(deviceName)),
          body: this.i18n('updatingInfo'),
          footer: this.i18n('installing', percentage),
        },
      ],
      [
        UpdateState.kRestarting,
        {
          title: this.i18n(
              'restartingTitleText', mojoString16ToString(deviceName)),
          body: this.i18n('restartingBodyText'),
          footer: this.i18n('restartingFooterText'),
        },
      ],
      [
        UpdateState.kFailed,
        {
          title: this.i18n(
              'updateFailedTitleText', mojoString16ToString(deviceName)),
          body: this.i18n('updateFailedBodyText'),
          footer: '',
        },
      ],
      [
        UpdateState.kSuccess,
        {
          title: this.i18n('deviceUpToDate', mojoString16ToString(deviceName)),
          body: this.i18n(
              'hasBeenUpdated', mojoString16ToString(deviceName),
              deviceVersion),
          footer: '',
        },
      ],
    ]);

    assert(dialogContent.has(state));
    return dialogContent.get(state) as DialogContent;
  }

  computeDialogContent(): DialogContent {
    // No update in progress.
    if (!this.isInitiallyInflight && !this.update) {
      return initialDialogContent;
    }

    if (inactiveDialogStates.includes(this.installationProgress.state) ||
        this.isDeviceRestarting()) {
      return this.createDialogContentObj(UpdateState.kRestarting);
    }

    // Regular case: Update is in progress, started from the same instance of
    // which the app launched.
    // Edge case: App launch with an update in progress, but no progress
    // callback has been called yet.
    if (this.isInitiallyInflight || this.isUpdateInProgress()) {
      return this.createDialogContentObj(UpdateState.kUpdating);
    }

    if (this.isUpdateDone()) {
      return this.createDialogContentObj(this.installationProgress.state);
    }
    return initialDialogContent;
  }

  protected isInIndeterminateState(): boolean {
    if (this.installationProgress) {
      return inactiveDialogStates.includes(this.installationProgress.state) ||
          this.isDeviceRestarting();
    }

    return false;
  }

  protected computeButtonText(): string {
    if (!this.isUpdateDone()) {
      return '';
    }

    return this.installationProgress.state === UpdateState.kSuccess ?
        this.i18n('doneButton') :
        this.i18n('okButton');
  }

  protected isDialogOpen(): boolean {
    assert(this.shadowRoot);
    return !!this.shadowRoot.querySelector('#updateDialog');
  }

  /** Event callback for 'open-update-dialog'. */
  private onOpenUpdateDialog(e: CustomEvent<OpenUpdateDialogEventDetail>):
      void {
    this.update = e.detail.update;
    this.isInitiallyInflight = e.detail.inflight;
    this.prepareForUpdate();
  }

  setIsInitiallyInflightForTesting(isInitiallyInflight: boolean): void {
    this.isInitiallyInflight = isInitiallyInflight;
  }
}

declare global {
  interface HTMLElementEventMap {
    'open-update-dialog': CustomEvent<OpenUpdateDialogEventDetail>;
  }

  interface HTMLElementTagNameMap {
    [FirmwareUpdateDialogElement.is]: FirmwareUpdateDialogElement;
  }
}

customElements.define(
    FirmwareUpdateDialogElement.is, FirmwareUpdateDialogElement);
