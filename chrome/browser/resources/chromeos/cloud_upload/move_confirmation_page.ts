// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';

import type {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import type {LottieRenderer} from 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {MetricsRecordedSetupPage, OperationType, UserAction} from './cloud_upload.mojom-webui.js';
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
  private animationPlayer: LottieRenderer|undefined;
  private playPauseButton: HTMLElement|undefined;

  // Save reference to listener so it can be removed from the document in
  // disconnectedCallback().
  private boundKeyDownListener_: (e: KeyboardEvent) => void;

  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});

    shadowRoot.innerHTML = getTemplate();
    const actionButton = this.$('.action-button')!;
    const cancelButton = this.$('.cancel-button')!;
    this.playPauseButton = this.$('#playPauseIcon')!;

    actionButton.addEventListener('click', () => this.onActionButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
    this.playPauseButton.addEventListener(
        'click', () => this.onPlayPauseButtonClick());
    this.boundKeyDownListener_ = this.onKeyDown.bind(this);
  }

  connectedCallback(): void {
    document.addEventListener('keydown', this.boundKeyDownListener_);
  }

  disconnectedCallback(): void {
    document.removeEventListener('keydown', this.boundKeyDownListener_);
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

    const isCopyOperation = operationType === OperationType.kCopy;
    const isPlural = numFiles > 1;
    const providerName = this.getProviderName(this.cloudProvider);

    // Animation.
    this.updateAnimation();

    // Title.
    const titleElement = this.$<HTMLElement>('#title')!;
    if (isCopyOperation) {
      titleElement.innerText = loadTimeData.getStringF(
          isPlural ? 'moveConfirmationCopyTitlePlural' :
                     'moveConfirmationCopyTitle',
          providerName,
          numFiles.toString(),
      );
    } else {
      titleElement.innerText = loadTimeData.getStringF(
          isPlural ? 'moveConfirmationMoveTitlePlural' :
                     'moveConfirmationMoveTitle',
          providerName, numFiles.toString());
    }

    // Checkbox and Body.
    const bodyText = this.$('#body-text');
    const checkbox = this.$<CrCheckboxElement>('#always-copy-or-move-checkbox');
    checkbox.innerText = loadTimeData.getString('moveConfirmationAlwaysMove');
    if (this.cloudProvider === CloudProvider.ONE_DRIVE) {
      bodyText.innerText =
          loadTimeData.getString('moveConfirmationOneDriveBodyText');

      // Only show checkbox if the confirmation has been shown before for
      // OneDrive.
      if (officeMoveConfirmationShownForOneDrive) {
        checkbox.checked = alwaysMoveToOneDrive;
      } else {
        checkbox!.remove();
      }
    } else {
      bodyText.innerText =
          loadTimeData.getStringF('moveConfirmationGoogleDriveBodyText');

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
    actionButton.innerText =
        loadTimeData.getString(isCopyOperation ? 'copyAndOpen' : 'moveAndOpen');
  }

  private getProviderName(cloudProvider: CloudProvider) {
    if (cloudProvider === CloudProvider.ONE_DRIVE) {
      return loadTimeData.getString('oneDrive');
    }
    return loadTimeData.getString('googleDrive');
  }

  private createAnimation(animationUrl: string) {
    this.animationPlayer = document.createElement('cros-lottie-renderer');
    this.animationPlayer.id = 'animation';
    this.animationPlayer.setAttribute('asset-url', animationUrl);
    this.animationPlayer.setAttribute('dynamic', 'true');
    this.animationPlayer.setAttribute('aria-hidden', 'true');
    this.animationPlayer.autoplay = true;
    const animationWrapper = this.$<HTMLElement>('.animation-wrapper')!;
    const playPauseIcon = this.$<HTMLElement>('#playPauseIcon')!;
    animationWrapper.insertBefore(this.animationPlayer, playPauseIcon);
  }

  private updateAnimation() {
    const provider =
        this.cloudProvider === CloudProvider.ONE_DRIVE ? 'onedrive' : 'drive';
    const animationUrl = `animations/move_confirmation_${provider}.json`;
    if (!this.animationPlayer) {
      this.createAnimation(animationUrl);
    } else {
      this.animationPlayer.setAttribute('asset-url', animationUrl);
    }
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
    if (this.cloudProvider === CloudProvider.ONE_DRIVE) {
      this.proxy.handler.recordCancel(
          MetricsRecordedSetupPage.kMoveConfirmationOneDrive);
      this.proxy.handler.respondWithUserActionAndClose(
          UserAction.kCancelOneDrive);
    } else {
      this.proxy.handler.recordCancel(
          MetricsRecordedSetupPage.kMoveConfirmationGoogleDrive);
      this.proxy.handler.respondWithUserActionAndClose(
          UserAction.kCancelGoogleDrive);
    }
  }

  private onPlayPauseButtonClick(): void {
    const animation = this.$<LottieRenderer>('#animation')!;
    const shouldPlay = this.playPauseButton!.className === 'play';
    if (shouldPlay) {
      animation.play();
      // Update button to Pause.
      this.playPauseButton!.className = 'pause';
      this.playPauseButton!.ariaLabel =
          loadTimeData.getString('animationPauseText');
    } else {
      animation.pause();
      // Update button to Play.
      this.playPauseButton!.className = 'play';
      this.playPauseButton!.ariaLabel =
          loadTimeData.getString('animationPlayText');
    }
  }

  private onKeyDown(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      // Handle Escape as a "cancel".
      e.stopImmediatePropagation();
      e.preventDefault();
      this.onCancelButtonClick();
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'move-confirmation-page': MoveConfirmationPageElement;
  }
}

customElements.define('move-confirmation-page', MoveConfirmationPageElement);
