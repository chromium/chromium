// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './error_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import 'chrome://borealis-installer/strings.m.js';
import 'chrome://resources/ash/common/cr.m.js';
import 'chrome://resources/ash/common/event_target.js';

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


import {getTemplate} from './app.html.js';
import {PageCallbackRouter} from './borealis_installer.mojom-webui.js';
import {InstallResult} from './borealis_types.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import type {BorealisInstallerErrorDialogElement} from './error_dialog.js';

const State = {
  WELCOME: 'welcome',
  INSTALLING: 'installing',
  COMPLETED: 'completed',
};

export interface BorealisInstallerAppElement {
  $: {
    errorDialog: BorealisInstallerErrorDialogElement,
    installLaunch: CrButtonElement,
  };
}

/**
 * @fileoverview
 * Dialog to install borealis.
 */
export class BorealisInstallerAppElement extends PolymerElement {
  static get is(): string {
    return 'borealis-installer-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      state: {
        type: String,
        value: State.WELCOME,
      },
    };
  }

  private listenerIds: number[];
  private router: PageCallbackRouter;
  private state: string;
  private installerProgress: number;
  private progressLabel: string;
  private canceling: boolean = false;

  constructor() {
    super();
    this.listenerIds = [];
    this.router = BrowserProxy.getInstance().callbackRouter;
  }

  override ready() {
    super.ready();
    this.addEventListener('retry', this.onErrorRetry);
    this.addEventListener('cancel', this.onErrorCancel);
    this.addEventListener('storage', this.onOpenStorage);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds.push(
        this.router.onProgressUpdate.addListener(
            (progressFraction: number, progressLabel: string) => {
              // Multiply by 100 to get percentage.
              this.installerProgress = Math.round(progressFraction * 100);
              this.progressLabel = progressLabel;
            }),
        this.router.onInstallFinished.addListener(
            (installResult: InstallResult) => {
              this.handleInstallResult(installResult);
            }),
        // Called when the user closes the installer (e.g. from the window bar)
        this.router.requestClose.addListener(() => {
          this.cancelAndClose();
        }));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(id => this.router.removeListener(id));
  }

  private onErrorRetry() {
    this.startInstall();
  }

  private onErrorCancel() {
    this.cancelAndClose();
  }

  private onOpenStorage() {
    BrowserProxy.getInstance().handler.openStoragePage();
    this.cancelAndClose();
  }

  private handleInstallResult(installResult: InstallResult) {
    switch (installResult) {
      case InstallResult.kSuccess:
        this.state = State.COMPLETED;
        this.$.installLaunch.focus();
        break;
      case InstallResult.kCancelled:
        this.cancelAndClose();
        break;
      default:
        this.$.errorDialog.show(installResult);
    }
  }

  protected eq(value1: any, value2: any): boolean {
    return value1 === value2;
  }

  protected getTitle(): string {
    let titleId: string = '';
    switch (this.state) {
      case State.WELCOME:
        titleId = 'confirmationTitle';
        break;
      case State.INSTALLING:
        titleId = 'ongoingTitle';
        break;
      case State.COMPLETED:
        titleId = 'finishedTitle';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getString(titleId);
  }

  protected getMessage(): string {
    let messageId: string = '';
    switch (this.state) {
      case State.WELCOME:
        messageId = 'confirmationMessage';
        break;
      case State.INSTALLING:
        messageId = 'ongingMessage';
        break;
      case State.COMPLETED:
        messageId = 'finishedMessage';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getString(messageId);
  }

  protected getProgressMessage(): string {
    return loadTimeData.getStringF('percent', this.installerProgress);
  }

  protected getProgressLabel(): string {
    return this.progressLabel;
  }

  protected shouldShowInstallOrLaunchButton(state: string): boolean {
    return [State.WELCOME, State.COMPLETED].includes(state);
  }

  protected getInstallOrLaunchLabel(state: string): string {
    if (state === State.COMPLETED) {
        return loadTimeData.getString('launch');
    }
    return loadTimeData.getString('install');
  }

  protected getCancelOrCloseLabel(state: string): string {
    if (state === State.COMPLETED) {
      return loadTimeData.getString('close');
    }
    return loadTimeData.getString('cancel');
  }

  protected onCancelButtonClicked(): void {
    this.cancelAndClose();
  }

  cancelAndClose(): void {
    if (this.canceling) {
      return;
    }
    this.canceling = true;
    switch (this.state) {
      case State.INSTALLING:
        BrowserProxy.getInstance().handler.cancelInstall();
        break;
      case State.COMPLETED:
        BrowserProxy.getInstance().handler.shutDown();
        break;
      default:
        break;
    }
    this.closePage();
  }

  protected onInstallOrLaunchButtonClicked(): void {
    switch (this.state) {
      case State.WELCOME:
        // 'Install' button clicked.
        this.startInstall();
        break;
      case State.COMPLETED:
        // 'Open Steam' button clicked.
        BrowserProxy.getInstance().handler.launch();
        this.closePage();
        break;
    }
  }

  startInstall(): void {
    this.installerProgress = 0;
    this.progressLabel = '';
    this.state = State.INSTALLING;
    BrowserProxy.getInstance().handler.install();
  }

  closePage(): void {
    BrowserProxy.getInstance().handler.onPageClosed();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'borealis-installer-app': BorealisInstallerAppElement;
  }
}

customElements.define(
    BorealisInstallerAppElement.is, BorealisInstallerAppElement);
