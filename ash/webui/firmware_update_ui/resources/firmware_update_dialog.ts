// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './firmware_shared.css.js';
import './firmware_shared_fonts.css.js';
import './firmware_update.mojom-webui.js';
import './strings.m.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeviceRequest, DeviceRequestId, DeviceRequestKind, DeviceRequestObserverInterface, DeviceRequestObserverReceiver, FirmwareUpdate, InstallationProgress, InstallControllerRemote, UpdateProgressObserverInterface, UpdateProgressObserverReceiver, UpdateState} from './firmware_update.mojom-webui.js';
import {getTemplate} from './firmware_update_dialog.html.js';
import {DialogContent, OpenUpdateDialogEventDetail} from './firmware_update_types.js';
import {isAppV2Enabled} from './firmware_update_utils.js';
import {getUpdateProvider} from './mojo_interface_provider.js';

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
    implements UpdateProgressObserverInterface, DeviceRequestObserverInterface {
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
            'isInitiallyInflight, lastDeviceRequestId)',
      },

      updateIsDone: {
        type: Boolean,
        value: false,
        computed: 'isUpdateDone(installationProgress.state)',
        reflectToAttribute: true,
      },

      /**
       * This property is used to keep track of the ID of the last-received
       * DeviceRequest. If this property is not null, it means there is a
       * pending request. If the property is null, it means there are no pending
       * requests.
       */
      lastDeviceRequestId: {
        type: Object,
        value: null,
      },
    };
  }

  update: FirmwareUpdate|null = null;
  installationProgress: InstallationProgress;
  private isInitiallyInflight = false;
  private lastDeviceRequestId: DeviceRequestId|null = null;
  dialogContent = initialDialogContent;
  private updateProvider = getUpdateProvider();
  private installController: InstallControllerRemote|null = null;
  private updateProgressObserverReceiver: UpdateProgressObserverReceiver|null =
      null;
  private deviceRequestObserverReceiver: DeviceRequestObserverReceiver|null =
      null;
  private inactiveDialogStates: UpdateState[] =
      [UpdateState.kUnknown, UpdateState.kIdle];


  override connectedCallback() {
    super.connectedCallback();

    // When v2 of the app is not enabled, treat "kWaitingForUser" as an inactive
    // state. This gracefully handles an unexpected edge case where fwupd sends
    // a kWaitingForUser status even though the v2 flag is disabled (which
    // shouldn't normally happen).
    if (!isAppV2Enabled()) {
      this.inactiveDialogStates.push(UpdateState.kWaitingForUser);
    }

    window.addEventListener(
        'open-update-dialog',
        (e) => this.onOpenUpdateDialog(
            e as CustomEvent<OpenUpdateDialogEventDetail>));
  }

  /** Implements DeviceRequestObserver.onDeviceRequest */
  onDeviceRequest(request: DeviceRequest): void {
    // OnDeviceRequest should only be triggered when the v2 flag is enabled.
    assert(isAppV2Enabled());

    if (request.kind !== DeviceRequestKind.kImmediate) {
      // Ignore non-immediate requests.
      return;
    }

    this.lastDeviceRequestId = request.id;
  }

  /** Implements UpdateProgressObserver.onStatusChanged */
  onStatusChanged(update: InstallationProgress): void {
    // If the update switched *away* from kWaitingForUser, hide any requests.
    // This can happen as part of the normal flow (i.e. the request was executed
    // by the user) or as part of an error flow (e.g. the instruction timed out,
    // or some other error occurred). In either case, we want to reset
    // lastDeviceRequestId so that the app knows the hide the request.
    if (isAppV2Enabled() && update.state !== UpdateState.kWaitingForUser &&
        this.installationProgress.state === UpdateState.kWaitingForUser) {
      this.lastDeviceRequestId = null;
    }
    if (update.state === UpdateState.kSuccess ||
        update.state === UpdateState.kFailed) {
      // Install is completed, reset inflight state.
      this.isInitiallyInflight = false;
    }
    this.installationProgress = update;
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
    this.installController.addUpdateProgressObserver(
        this.updateProgressObserverReceiver.$.bindNewPipeAndPassRemote());

    // Listen for device requests if v2 of the app is enabled.
    if (isAppV2Enabled()) {
      this.deviceRequestObserverReceiver =
          new DeviceRequestObserverReceiver(this);
      this.installController.addDeviceRequestObserver(
          this.deviceRequestObserverReceiver.$.bindNewPipeAndPassRemote());
    }

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

    if (isAppV2Enabled()) {
      activeDialogStates.push(UpdateState.kWaitingForUser);
    }

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
    if (this.inactiveDialogStates.includes(this.installationProgress.state)) {
      return this.installationProgress.percentage > 0;
    }

    return this.installationProgress.state === UpdateState.kUpdating;
  }

  protected isDeviceRestarting(): boolean {
    return this.installationProgress.state === UpdateState.kRestarting;
  }

  protected shouldShowProgressBar(): boolean {
    const showProgressBar = this.isUpdateInProgress() ||
        this.isDeviceRestarting() || this.isWaitingForUserAction() ||
        this.isInitiallyInflight;
    assert(this.shadowRoot);
    const progressIsActiveEl = this.shadowRoot.activeElement ==
        this.shadowRoot.querySelector('#progress');
    // Move focus to the dialog title if the progress label is currently
    // active and set to be hidden. This case is reached when the dialog state
    // moves from restarting to completed.
    const dialogTitle =
        this.shadowRoot.querySelector<HTMLElement>('#updateDialogTitle');
    if (progressIsActiveEl && !showProgressBar && dialogTitle) {
      dialogTitle.focus();
    }
    return showProgressBar;
  }

  protected isUpdateDone(): boolean {
    return this.installationProgress.state === UpdateState.kSuccess ||
        this.installationProgress.state === UpdateState.kFailed;
  }

  createRequestDialogContent(): DialogContent {
    assert(this.update);
    const {deviceName} = this.update;
    const {percentage} = this.installationProgress;
    assert(this.lastDeviceRequestId !== null);

    const deviceNameString: string = mojoString16ToString(deviceName);

    return {
      title: this.i18n('updating', deviceNameString),
      body: this.getI18nStringForDeviceRequestId(
          this.lastDeviceRequestId, deviceNameString),
      footer: this.i18n('waitingFooterText', percentage),
    };
  }

  createDialogContentObj(state: UpdateState): DialogContent {
    assert(this.update);
    const {deviceName, deviceVersion, needsReboot} = this.update;
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
    ]);

    if (needsReboot) {
      dialogContent.set(UpdateState.kSuccess, {
        title: this.i18n(
            'deviceReadyToInstallUpdate', mojoString16ToString(deviceName)),
        body: this.i18n(
            'deviceNeedsReboot', mojoString16ToString(deviceName),
            deviceVersion),
        footer: '',
      });
    } else {
      dialogContent.set(UpdateState.kSuccess, {
        title: this.i18n('deviceUpToDate', mojoString16ToString(deviceName)),
        body: this.i18n(
            'hasBeenUpdated', mojoString16ToString(deviceName), deviceVersion),
        footer: '',
      });
    }

    assert(dialogContent.has(state));
    return dialogContent.get(state) as DialogContent;
  }

  computeDialogContent(): DialogContent {
    // No update in progress.
    if (!this.isInitiallyInflight && !this.update) {
      return initialDialogContent;
    }

    if (this.inactiveDialogStates.includes(this.installationProgress.state) ||
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

    if (isAppV2Enabled() &&
        this.installationProgress.state === UpdateState.kWaitingForUser) {
      if (this.lastDeviceRequestId === null) {
        // Show normal update flow until onDeviceRequest is called.
        return this.createDialogContentObj(UpdateState.kUpdating);
      } else {
        return this.createRequestDialogContent();
      }
    }

    if (this.isUpdateDone()) {
      return this.createDialogContentObj(this.installationProgress.state);
    }
    return initialDialogContent;
  }

  protected isInIndeterminateState(): boolean {
    if (this.installationProgress) {
      return this.inactiveDialogStates.includes(
                 this.installationProgress.state) ||
          this.isDeviceRestarting();
    }

    return false;
  }

  protected isProgressBarDisabled(): boolean {
    return this.isWaitingForUserAction();
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

  private isWaitingForUserAction(): boolean {
    return isAppV2Enabled() && this.lastDeviceRequestId !== null &&
        this.installationProgress.state === UpdateState.kWaitingForUser;
  }

  private getDialogBodyAriaLive(): string {
    // Use assertive aria-live value to ensure user requests are announced
    // before they time out.
    return this.isWaitingForUserAction() ? 'assertive' : '';
  }

  private getStringIdForDeviceRequestId(deviceRequestId: DeviceRequestId):
      string {
    switch (deviceRequestId) {
      case (DeviceRequestId.kDoNotPowerOff):
        return 'requestIdDoNotPowerOff';
      case (DeviceRequestId.kReplugInstall):
        return 'requestIdReplugInstall';
      case (DeviceRequestId.kInsertUSBCable):
        return 'requestIdInsertUsbCable';
      case (DeviceRequestId.kRemoveUSBCable):
        return 'requestIdRemoveUsbCable';
      case (DeviceRequestId.kPressUnlock):
        return 'requestIdPressUnlock';
      case (DeviceRequestId.kRemoveReplug):
        return 'requestIdRemoveReplug';
      case (DeviceRequestId.kReplugPower):
        return 'requestIdReplugPower';
    }
  }

  private getI18nStringForDeviceRequestId(
      deviceRequestId: DeviceRequestId, deviceName: string): string {
    const requestStringId = this.getStringIdForDeviceRequestId(deviceRequestId);

    // DoNotPowerOff request does not use the device name.
    if (deviceRequestId == DeviceRequestId.kDoNotPowerOff) {
      return this.i18n(requestStringId);
    }

    return this.i18n(requestStringId, deviceName);
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
