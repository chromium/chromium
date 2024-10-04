// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'local-files-migration-dialog' defines the UI for the SkyVault migration
 * workflow.
 */

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './skyvault_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {LocalFilesBrowserProxy} from './local_files_browser_proxy.js';
import {CloudProvider, TimeUnit, TimeUnitAndValue} from './local_files_migration.mojom-webui.js';
import {getTemplate} from './local_files_migration_dialog.html.js';

class LocalFilesMigrationDialogElement extends HTMLElement {
  private proxy: LocalFilesBrowserProxy = LocalFilesBrowserProxy.getInstance();
  private cloudProvider: CloudProvider|null = null;

  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = getTemplate();

    const uploadNowButton = this.$('#upload-now-button');
    uploadNowButton.addEventListener(
        'click', () => this.onUploadNowButtonClicked_());
    const dismissButton = this.$('#dismiss-button');
    dismissButton.addEventListener(
        'click', () => this.onDismissButtonClicked_());

    this.proxy.callbackRouter.updateRemainingTime.addListener(
        (remainingTime: TimeUnitAndValue) =>
            this.updateRemainingTime_(remainingTime));
  }

  async connectedCallback() {
    try {
      const {cloudProvider, remainingTime, startDateAndTime} =
          await this.proxy.handler.getInitialDialogInfo();
      this.initializeDialog_(cloudProvider, remainingTime, startDateAndTime);
    } catch (error) {
      console.error('Error fetching initial dialog info:', error);
    }
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  private initializeDialog_(
      cloudProvider: CloudProvider, remainingTime: TimeUnitAndValue,
      startDateAndTime: string) {
    this.cloudProvider = cloudProvider;
    const providerName = this.getCloudProviderName_(cloudProvider);

    const startMessage = this.$('#start-message');
    startMessage.innerText = loadTimeData.getStringF(
        'uploadStartMessage', providerName, startDateAndTime);

    const doneMessage = this.$('#done-message');
    doneMessage.innerText =
        loadTimeData.getStringF('uploadDoneMessage', providerName);

    this.updateRemainingTime_(remainingTime);
  }

  private async onUploadNowButtonClicked_() {
    this.proxy.handler.uploadNow();
  }

  private onDismissButtonClicked_() {
    this.proxy.handler.close();
  }

  private updateRemainingTime_(remainingTime: TimeUnitAndValue) {
    if (this.cloudProvider === null) {
      console.error(
          'CloudProvider should be set before calling UpdateRemainingTime()');
      return;
    }
    if (remainingTime.value < 0) {
      console.error(
          `Remaining time must be positive, got ${remainingTime.value}`);
      return;
    }

    const title = this.$('#title-text');
    const dismissButton = this.$('#dismiss-button');
    const providerName = this.getCloudProviderName_(this.cloudProvider);
    const remainingTimeValue = remainingTime.value;
    const plural = remainingTimeValue > 1;
    switch (remainingTime.unit) {
      case TimeUnit.kHours:
        title.innerText = plural ?
            loadTimeData.getStringF(
                'titleHours', providerName, remainingTimeValue) :
            loadTimeData.getStringF('titleHour', providerName);
        dismissButton.innerText =
            loadTimeData.getStringF('uploadInHours', remainingTimeValue);
        break;
      case TimeUnit.kMinutes:
        title.innerText = plural ?
            loadTimeData.getStringF(
                'titleMinutes', providerName, remainingTimeValue) :
            loadTimeData.getStringF('titleMinute', providerName);
        dismissButton.innerText =
            loadTimeData.getStringF('uploadInMinutes', remainingTimeValue);
        break;
    }
  }

  private getCloudProviderName_(cloudProvider: CloudProvider) {
    switch (cloudProvider) {
      case CloudProvider.kOneDrive:
        return loadTimeData.getString('oneDrive');
      case CloudProvider.kGoogleDrive:
        return loadTimeData.getString('googleDrive');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    ['local-files-migration-dialog']: LocalFilesMigrationDialogElement;
  }
}

customElements.define(
    'local-files-migration-dialog', LocalFilesMigrationDialogElement);
