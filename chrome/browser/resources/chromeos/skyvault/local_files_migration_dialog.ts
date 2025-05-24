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
import '/strings.m.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {LocalFilesBrowserProxy} from './local_files_browser_proxy.js';
import type {TimeUnitAndValue} from './local_files_migration.mojom-webui.js';
import {CloudProvider, TimeUnit} from './local_files_migration.mojom-webui.js';
import {getTemplate} from './local_files_migration_dialog.html.js';

class LocalFilesMigrationDialogElement extends HTMLElement {
  private proxy: LocalFilesBrowserProxy = LocalFilesBrowserProxy.getInstance();
  private cloudProvider: CloudProvider|null = null;
  private dialog: CrDialogElement;
  private dismissButton: CrButtonElement;
  private uploadOrDeleteNowButton: CrButtonElement;

  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = getTemplate();

    this.dialog = this.$('cr-dialog');

    this.uploadOrDeleteNowButton = this.$('#upload-or-delete-now-button');
    this.uploadOrDeleteNowButton.addEventListener(
        'click', () => this.onUploadOrDeleteNowButtonClicked_());
    this.dismissButton = this.$('#dismiss-button');
    this.dismissButton.addEventListener(
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

    const startMessage = this.$('#start-message');
    const doneMessage = this.$('#done-message');

    if (this.cloudProvider === CloudProvider.kDelete) {
      startMessage.innerText =
          loadTimeData.getStringF('deleteStartMessage', startDateAndTime);
      doneMessage.innerText = loadTimeData.getString('deleteStoreMessage');
      this.uploadOrDeleteNowButton.innerText =
          loadTimeData.getStringF('deleteNow');
    } else {
      const providerName = this.getCloudProviderName_(cloudProvider);
      startMessage.innerText = loadTimeData.getStringF(
          'uploadStartMessage', providerName, startDateAndTime);
      doneMessage.innerText =
          loadTimeData.getStringF('uploadDoneMessage', providerName);
      this.uploadOrDeleteNowButton.innerText =
          loadTimeData.getStringF('uploadNow');
    }
    this.updateRemainingTime_(remainingTime);
    // Show after all the text is ready, so that screen readers can properly
    // access it.
    this.dialog.showModal();
  }

  private onUploadOrDeleteNowButtonClicked_() {
    this.proxy.handler.uploadOrDeleteNow();
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
    try {
      const titleText = (this.cloudProvider === CloudProvider.kDelete) ?
          this.getDeleteTitleText_(remainingTime) :
          this.getCloudProviderTitleText_(remainingTime);
      const title = this.$('#header');
      title.innerText = `${titleText}`;
      this.dialog.setTitleAriaLabel(titleText);
    } catch (error) {
      console.error('Error setting the dialog title: ', error);
    }

    this.dismissButton.innerText = this.getDismissButtonText_(remainingTime);
  }

  private getCloudProviderTitleText_(remainingTime: TimeUnitAndValue): string {
    if (this.cloudProvider === null) {
      throw new Error(
          'Function should only be called after cloudProvider is set');
    }
    if (this.cloudProvider === CloudProvider.kDelete) {
      throw new Error('Function should not be called for the Delete option');
    }
    let providerName: string;
    try {
      providerName = this.getCloudProviderName_(this.cloudProvider);
    } catch (error) {
      throw error;
    }
    const remainingTimeValue = remainingTime.value;
    const plural = remainingTimeValue > 1;

    switch (remainingTime.unit) {
      case TimeUnit.kHours:
        return plural ?
            loadTimeData.getStringF(
                'uploadTitleHours', providerName, remainingTimeValue) :
            loadTimeData.getStringF('uploadTitleHour', providerName);
      case TimeUnit.kMinutes:
        return plural ?
            loadTimeData.getStringF(
                'uploadTitleMinutes', providerName, remainingTimeValue) :
            loadTimeData.getStringF('uploadTitleMinute', providerName);
    }
  }

  private getDeleteTitleText_(remainingTime: TimeUnitAndValue): string {
    const remainingTimeValue = remainingTime.value;
    const plural = remainingTimeValue > 1;

    switch (remainingTime.unit) {
      case TimeUnit.kHours:
        return plural ?
            loadTimeData.getStringF('deleteTitleHours', remainingTimeValue) :
            loadTimeData.getString('deleteTitleHour');
      case TimeUnit.kMinutes:
        return plural ?
            loadTimeData.getStringF('deleteTitleMinutes', remainingTimeValue) :
            loadTimeData.getString('deleteTitleMinute');
    }
  }

  private getDismissButtonText_(remainingTime: TimeUnitAndValue) {
    const remainingTimeValue = remainingTime.value;
    const prefix =
        this.cloudProvider === CloudProvider.kDelete ? 'deleteIn' : 'uploadIn';
    switch (remainingTime.unit) {
      case TimeUnit.kHours:
        return loadTimeData.getStringF(`${prefix}Hours`, remainingTimeValue);
      case TimeUnit.kMinutes:
        return loadTimeData.getStringF(`${prefix}Minutes`, remainingTimeValue);
    }
  }

  private getCloudProviderName_(cloudProvider: CloudProvider) {
    switch (cloudProvider) {
      case CloudProvider.kOneDrive:
        return loadTimeData.getString('oneDrive');
      case CloudProvider.kGoogleDrive:
        return loadTimeData.getString('googleDrive');
      case CloudProvider.kDelete:
        throw new Error('Function should not be called for the Delete option');
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
