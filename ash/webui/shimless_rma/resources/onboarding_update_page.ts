// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_update_page.html.js';
import {HardwareVerificationStatusObserverReceiver, OsUpdateObserverReceiver, OsUpdateOperation, ShimlessRmaServiceInterface, StateResult, UpdateErrorCode} from './shimless_rma.mojom-webui.js';
import {disableAllButtons, enableAllButtons, enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-update-page' is the page shown when there is an Chrome OS update
 * available on the device for the user to install before the RMA process.
 */

const OnboardingUpdatePageElementBase = I18nMixin(PolymerElement);

export class OnboardingUpdatePageElement extends
    OnboardingUpdatePageElementBase {
  static get is() {
    return 'onboarding-update-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.ts.
       */
      allButtonsDisabled: Boolean,

      currentVersionText: {
        type: String,
        value: '',
      },

      updateVersionButtonLabel: {
        type: String,
        value: '',
      },

      updateInProgress: {
        type: Boolean,
        value: false,
        observer:
            OnboardingUpdatePageElement.prototype.onUpdateInProgressChange,
      },

      verificationFailedMessage: {
        type: String,
        value: '',
      },

      /**
       * A string containing a list of the unqualified component identifiers
       * separated by new lines.
       */
      unqualifiedComponentsText: {
        type: String,
        value: '',
      },


      osUpdateEncounteredError: {
        type: Boolean,
        value: false,
      },
    };
  }

  allButtonsDisabled: boolean;
  shimlessRmaService: ShimlessRmaServiceInterface;
  isCompliant: boolean;
  protected currentVersionText: string;
  protected updateVersionButtonLabel: string;
  protected updateInProgress: boolean;
  protected verificationFailedMessage: TrustedHTML;
  protected unqualifiedComponentsText: string;
  protected osUpdateEncounteredError: boolean;
  protected currentVersion: string;
  protected osUpdateObserverReceiver: OsUpdateObserverReceiver|null;
  protected hwVerificationObserverReceiver: HardwareVerificationStatusObserverReceiver|null;

  constructor() {
    super();
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    this.shimlessRmaService = getShimlessRmaService();
    this.currentVersion = '';
    this.osUpdateObserverReceiver = new OsUpdateObserverReceiver(this);

    this.shimlessRmaService.observeOsUpdateProgress(
        this.osUpdateObserverReceiver.$.bindNewPipeAndPassRemote());

    // We assume it's compliant until updated in onHardwareVerificationResult().
    this.isCompliant = true;
    this.hwVerificationObserverReceiver = new HardwareVerificationStatusObserverReceiver(this);

    this.shimlessRmaService.observeHardwareVerificationStatus(
        this.hwVerificationObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.getCurrentVersionText();
    this.getUpdateVersionNumber();
    enableNextButton(this);

    focusPageTitle(this);
  }

  private getCurrentVersionText(): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shimlessRmaService.getCurrentOsVersion().then((res: {version: string|null}) => {
      if (res.version != null) {
        this.currentVersion = res.version;
      } else {
        this.currentVersion = '0.0.0.0';
      }
      this.currentVersionText =
          this.i18n('currentVersionOutOfDateText', this.currentVersion);
    });
  }

  private getUpdateVersionNumber(): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shimlessRmaService.checkForOsUpdates().then((res: {updateAvailable: boolean, version: string|null}) => {
      assert(res.updateAvailable);
      this.updateVersionButtonLabel =
          this.i18n('updateVersionRestartLabel', res?.version || '');
    });
  }

  private updateOs(): void {
    this.updateInProgress = true;
    this.shimlessRmaService.updateOs().then((res: {updateStarted: boolean}) => {
      if (!res.updateStarted) {
        this.updateInProgress = false;
      }
    });
  }

  protected onUpdateButtonClicked(): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    this.updateOs();
  }

  protected onRetryUpdateButtonClicked(): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    assert(this.osUpdateEncounteredError);
    this.osUpdateEncounteredError = false;

    this.updateOs();
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    return this.shimlessRmaService.updateOsSkipped();
  }

  /**
   * Implements OsUpdateObserver.onOsUpdateProgressUpdated()
   */
  onOsUpdateProgressUpdated(operation: OsUpdateOperation, _progress: number, error: UpdateErrorCode): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    // Ignore progress when not updating, it is just the update available check.
    if (!this.updateInProgress) {
      return;
    }

    if (operation === OsUpdateOperation.kIdle ||
        operation === OsUpdateOperation.kReportingErrorEvent ||
        operation === OsUpdateOperation.kNeedPermissionToUpdate ||
        operation === OsUpdateOperation.kDisabled) {
      this.updateInProgress = false;

      if (error !== UpdateErrorCode.kSuccess) {
        this.osUpdateEncounteredError = true;
      }
    }
  }

  /**
   * Implements
   * HardwareVerificationStatusObserver.onHardwareVerificationResult()
   */
  onHardwareVerificationResult(isCompliant: boolean, errorMessage: string): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.isCompliant = isCompliant;

    if (!this.isCompliant) {
      this.unqualifiedComponentsText = errorMessage;
      this.setVerificationFailedMessage();
    }
  }

  private setVerificationFailedMessage(): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.verificationFailedMessage = this.i18nAdvanced(
        'osUpdateUnqualifiedComponentsTopText', {attrs: ['id']});

    // The #unqualifiedComponentsLink identifier is sourced from the string
    // attached to `osUpdateUnqualifiedComponentsTopText` in the related .grd
    // file.
    const linkElement: HTMLAnchorElement|null =
        this.shadowRoot!.querySelector('#unqualifiedComponentsLink');
    assert(linkElement);
    linkElement.setAttribute('href', '#');
    const dialog: CrDialogElement|null = this.shadowRoot!.querySelector('#unqualifiedComponentsDialog');
    assert(dialog);
    linkElement.addEventListener('click', () => dialog.showModal());
  }

  private closeDialog(): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    const dialog: CrDialogElement|null = this.shadowRoot!.querySelector('#unqualifiedComponentsDialog');
    assert(dialog);
    dialog.close();
  }

  private onUpdateInProgressChange(): void {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    if (this.updateInProgress) {
      disableAllButtons(this, /*showBusyStateOverlay=*/ false);
    } else {
      enableAllButtons(this);
    }
  }

  protected shouldShowUpdateInstructions(): boolean {
    return !this.updateInProgress && !this.osUpdateEncounteredError;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingUpdatePageElement.is]: OnboardingUpdatePageElement;
  }
}

customElements.define(
    OnboardingUpdatePageElement.is, OnboardingUpdatePageElement);
