// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {OperationType, UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './move_confirmation_page.html.js';

export enum CloudProvider {
  GOOGLE_DRIVE,
  ONE_DRIVE,
}

/**
 * The MoveConfirmationPageElement represents the dialog page shown when the
 * user opens a file that needs to be moved first, and they haven't yet decided
 * to always move files.
 */
export class MoveConfirmationPageElement extends HTMLElement {
  private proxy: CloudUploadBrowserProxy =
      CloudUploadBrowserProxy.getInstance();
  private cloudProvider: CloudProvider|undefined;

  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});

    shadowRoot.innerHTML = getTemplate();
    const actionButton = this.$('.action-button')!;
    const cancelButton = this.$('.cancel-button')!;

    actionButton.addEventListener('click', () => this.onActionButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  async setDialogAttributes(
      numFiles: number, operationType: OperationType,
      cloudProvider: CloudProvider) {
    const [
      {moveConfirmationShown: officeMoveConfirmationShownForDrive},
      {alwaysMove: alwaysMoveToDrive},
      {moveConfirmationShown: officeMoveConfirmationShownForOneDrive},
      {alwaysMove: alwaysMoveToOneDrive},
    ] = await Promise.all([
      this.proxy.handler.getOfficeMoveConfirmationShownForDrive(),
      this.proxy.handler.getAlwaysMoveOfficeFilesToDrive(),
      this.proxy.handler.getOfficeMoveConfirmationShownForOneDrive(),
      this.proxy.handler.getAlwaysMoveOfficeFilesToOneDrive(),
    ]);

    this.cloudProvider = cloudProvider;

    const operationTypeText =
        operationType === OperationType.kCopy ? 'Copy' : 'Move';
    const filesText = numFiles > 1 ? 'files' : 'file';
    const name = this.getProviderName(this.cloudProvider);

    // Animation.
    this.updateAnimation(
        window.matchMedia('(prefers-color-scheme: dark)').matches);
    window.matchMedia('(prefers-color-scheme: dark)')
        .addEventListener('change', event => {
          this.updateAnimation(event.matches);
        });

    // Title.
    const titleElement = this.$<HTMLElement>('#title')!;
    titleElement.innerText = `${operationTypeText} ${numFiles.toString()} ${
        filesText} to ${name} to open?`;

    // Checkbox and Body.
    const bodyText = this.$('#body-text');
    const checkbox = this.$<CrCheckboxElement>('#always-copy-or-move-checkbox');
    checkbox.innerText = 'Don\'t ask again';
    if (this.cloudProvider === CloudProvider.ONE_DRIVE) {
      bodyText.innerText =
          'Microsoft 365 requires files to be stored in OneDrive. ' +
          'Local files will move and files from other locations will copy. ' +
          'Your files can be found in the Microsoft OneDrive folder in the ' +
          'Files app.';

      // Only show checkbox if the confirmation has been shown before for
      // OneDrive.
      if (officeMoveConfirmationShownForOneDrive) {
        checkbox.checked = alwaysMoveToOneDrive;
      } else {
        checkbox!.remove();
      }
    } else {
      bodyText.innerText =
          'Google Docs, Sheets, and Slides require files to be stored in ' +
          'Google Drive. Local files will move and files from other ' +
          'locations will copy. Your files can be found in the Google Drive ' +
          'folder in the Files app.';

      // Only show checkbox if the confirmation has been shown before for
      // Drive.
      if (officeMoveConfirmationShownForDrive) {
        checkbox.checked = alwaysMoveToDrive;
      } else {
        checkbox!.remove();
      }
    }

    // Action button.
    const actionButton = this.$<HTMLElement>('.action-button')!;
    actionButton.innerText = `${operationTypeText} and open`;
  }

  private getProviderName(cloudProvider: CloudProvider) {
    if (cloudProvider === CloudProvider.ONE_DRIVE) {
      return 'Microsoft OneDrive';
    }
    return 'Google Drive';
  }

  private updateAnimation(isDarkMode: boolean) {
    const provider =
        this.cloudProvider === CloudProvider.ONE_DRIVE ? 'onedrive' : 'drive';
    const colorScheme = isDarkMode ? 'dark' : 'light';
    const animationUrl =
        `animations/move_confirmation_${provider}_${colorScheme}.json`;
    this.shadowRoot!.querySelector('cr-lottie')!.setAttribute(
        'animation-url', animationUrl);
  }

  private onActionButtonClick(): void {
    const checkbox = this.$<CrCheckboxElement>('#always-copy-or-move-checkbox');
    const setAlwaysMove = !!(checkbox && checkbox.checked);
    if (this.cloudProvider === CloudProvider.ONE_DRIVE) {
      this.proxy.handler.setAlwaysMoveOfficeFilesToOneDrive(setAlwaysMove);
      this.proxy.handler.respondWithUserActionAndClose(
          UserAction.kUploadToOneDrive);
    } else {
      this.proxy.handler.setAlwaysMoveOfficeFilesToDrive(setAlwaysMove);
      this.proxy.handler.respondWithUserActionAndClose(
          UserAction.kUploadToGoogleDrive);
    }
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'move-confirmation-page': MoveConfirmationPageElement;
  }
}

customElements.define('move-confirmation-page', MoveConfirmationPageElement);
