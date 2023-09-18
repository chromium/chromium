// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import 'chrome://borealis-installer/strings.m.js';
import 'chrome://resources/ash/common/cr.m.js';
import 'chrome://resources/ash/common/event_target.js';

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PageCallbackRouter} from './borealis_installer.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';

const State = {
  WELCOME: 'welcome',
  INSTALLING: 'installing',
  COMPLETED: 'completed',
};

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

  constructor() {
    super();
    this.listenerIds = [];
    this.router = BrowserProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds.push(
        this.router.onProgressUpdate.addListener(
            (progressFraction: number, progressLabel: string) => {
              this.installerProgress = progressFraction * 100;
              this.progressLabel = progressLabel;
            }),
        this.router.onInstallFinished.addListener(() => {
          this.state = State.COMPLETED;
        }),
        // Called when retry is clicked from the error dialog.
        // Restarts the installation.
        this.router.restartInstallation.addListener(() => {
          this.startInstall();
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

  protected onCancelButtonClicked(): void {
    this.cancelAndClose();
  }

  cancelAndClose(): void {
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

  protected onInstallButtonClicked(): void {
    this.startInstall();
  }

  startInstall(): void {
    this.installerProgress = 0;
    this.progressLabel = '';
    this.state = State.INSTALLING;
    BrowserProxy.getInstance().handler.install();
  }

  protected onOpenButtonClicked(): void {
    BrowserProxy.getInstance().handler.launch();
    this.closePage();
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
