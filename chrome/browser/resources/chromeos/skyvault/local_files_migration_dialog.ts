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
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

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
    this.updateRemainingTime_(remainingTime);
    // TODO(b/334511998): Use i18n strings.
    const message = this.$('#message');
    message.innerText =
        `Your administrator will start uploading your files to ` +
        `${providerName} on ${startDateAndTime}. Keep your ` +
        'device connected until your file upload is complete. ' +
        'During the upload, you will only be able to read local files.';
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

    const title = this.$('#title');
    const dismissButton = this.$('#dismiss-button');
    const providerName = this.getCloudProviderName_(this.cloudProvider);
    const remainingTimeValue = remainingTime.value;
    const plural = remainingTimeValue > 1;
    switch (remainingTime.unit) {
      case TimeUnit.kHours:
        title.innerText = `Your file upload to ${providerName} will begin in ${
            remainingTimeValue} ${plural ? 'hours' : 'hour'}`;
        dismissButton.innerText = `Upload in ${remainingTimeValue} hr`;
        break;
      case TimeUnit.kMinutes:
        title.innerText = `Your file upload to ${providerName} will begin in ${
            remainingTimeValue} ${plural ? 'minutes' : 'minute'}`;
        dismissButton.innerText = `Upload in ${remainingTimeValue} min`;
        break;
    }
  }

  private getCloudProviderName_(cloudProvider: CloudProvider) {
    // TODO(b/334511998): Use i18n strings.
    switch (cloudProvider) {
      case CloudProvider.kOneDrive:
        return 'Microsoft OneDrive';
      case CloudProvider.kGoogleDrive:
        return 'Google Drive';
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
