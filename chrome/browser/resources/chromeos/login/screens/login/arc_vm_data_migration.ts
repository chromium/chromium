// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for ARCVM /data migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_icons.html.js';

import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {ArcVmDataMigrationPageCallbackRouter, ArcVmDataMigrationPageHandlerRemote} from '../../mojom-webui/screens_login.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './arc_vm_data_migration.html.js';

// Keep in sync with ArcVmDataMigrationPage_ArcVmUIState
enum ArcVmDataMigrationUiState {
  LOADING = 'loading',
  WELCOME = 'welcome',
  RESUM = 'resume',
  PROGRESS = 'progress',
  SUCCESS = 'success',
  FAILURE = 'failure',
}

const ArcVmDataMigrationScreenElementBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

export class ArcVmDataMigrationScreen extends
    ArcVmDataMigrationScreenElementBase {
  static get is() {
    return 'arc-vm-data-migration-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      hasEnoughFreeDiskSpace: {
        type: Boolean,
        value: true,
      },

      requiredFreeDiskSpaceInString: {
        type: String,
        value: '',
      },

      minimumBatteryPercent: {
        type: Number,
        value: 0,
      },

      hasEnoughBattery: {
        type: Boolean,
        value: true,
      },

      isConnectedToCharger: {
        type: Boolean,
        value: true,
      },

      migrationProgress: {
        type: Number,
        value: -1,
      },

      estimatedRemainingTimeInString: {
        type: String,
        value: '',
      },
    };
  }

  private hasEnoughFreeDiskSpace: boolean;
  private requiredFreeDiskSpaceInString: string;
  private minimumBatteryPercent: number;
  private hasEnoughBattery: boolean;
  private isConnectedToCharger: boolean;
  private migrationProgress: number;
  private estimatedRemainingTimeInString: string;
  private callbackRouter: ArcVmDataMigrationPageCallbackRouter;
  private handler: ArcVmDataMigrationPageHandlerRemote;

  constructor() {
    super();
    this.callbackRouter = new ArcVmDataMigrationPageCallbackRouter();
    this.handler = new ArcVmDataMigrationPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory
        .establishArcVmDataMigrationScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver())
        .then((response: any) => {
          this.callbackRouter.$.bindHandle(response.pending.handle);
        });
    this.callbackRouter.setUIState.addListener(this.setUIState.bind(this));
    this.callbackRouter.setRequiredFreeDiskSpace.addListener(
        this.setRequiredFreeDiskSpace.bind(this));
    this.callbackRouter.setMinimumBatteryPercent.addListener(
        this.setMinimumBatteryPercent.bind(this));
    this.callbackRouter.setBatteryState.addListener(
        this.setBatteryState.bind(this));
    this.callbackRouter.setMigrationProgress.addListener(
        this.setMigrationProgress.bind(this));
    this.callbackRouter.setEstimatedRemainingTime.addListener(
        this.setEstimatedRemainingTime.bind(this));
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return ArcVmDataMigrationUiState.LOADING;
  }

  override get UI_STEPS() {
    return ArcVmDataMigrationUiState;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('ArcVmDataMigrationScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.MIGRATION;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  setUIState(state: number): void {
    this.setUIStep(Object.values(ArcVmDataMigrationUiState)[state]);
  }

  setRequiredFreeDiskSpace(requiredFreeDiskSpaceInString: string): void {
    this.hasEnoughFreeDiskSpace = false;
    this.requiredFreeDiskSpaceInString = requiredFreeDiskSpaceInString;
  }

  setMinimumBatteryPercent(minimumBatteryPercent: number): void {
    this.minimumBatteryPercent = Math.floor(minimumBatteryPercent);
  }

  setBatteryState(hasEnoughBattery: boolean, isConnectedToCharger: boolean):
      void {
    this.hasEnoughBattery = hasEnoughBattery;
    this.isConnectedToCharger = isConnectedToCharger;
  }

  setMigrationProgress(migrationProgress: number): void {
    this.migrationProgress = Math.floor(migrationProgress);
  }

  setEstimatedRemainingTime(estimatedRemainingTimeInString: String16): void {
    this.estimatedRemainingTimeInString =
        String.fromCharCode(...estimatedRemainingTimeInString.data);
  }

  private shouldDisableUpdateButton(
      hasEnoughFreeDiskSpace: boolean, hasEnoughBattery: boolean): boolean {
    return !hasEnoughFreeDiskSpace || !hasEnoughBattery;
  }

  private isProgressIndeterminate(migrationProgress: number): boolean {
    return migrationProgress < 0;
  }

  private onSkipButtonClicked(): void {
    this.handler.onSkipClicked();
  }

  private onUpdateButtonClicked(): void {
    this.handler.onUpdateClicked();
  }

  private onResumeButtonClicked(): void {
    this.handler.onResumeClicked();
  }

  private onFinishButtonClicked(): void {
    this.handler.onFinishClicked();
  }

  private onReportButtonClicked(): void {
    this.handler.onReportClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ArcVmDataMigrationScreen.is]: ArcVmDataMigrationScreen;
  }
}

customElements.define(ArcVmDataMigrationScreen.is, ArcVmDataMigrationScreen);
