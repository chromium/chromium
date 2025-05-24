// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './icons.html.js';
import './shimless_rma_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ClickExitButtonEvent} from './events.js';
import {CLICK_EXIT_BUTTON, createCustomEvent} from './events.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_landing_page.html.js';
import type {ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {HardwareVerificationResult, HardwareVerificationStatusObserverReceiver} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

declare global {
  interface HTMLElementEventMap {
    [CLICK_EXIT_BUTTON]: ClickExitButtonEvent;
  }
}

/**
 * @fileoverview
 * 'onboarding-landing-page' is the main landing page for the shimless rma
 * process.
 */

const OnboardingLandingPageBase = I18nMixin(PolymerElement);

export class OnboardingLandingPage extends OnboardingLandingPageBase {
  static get is() {
    return 'onboarding-landing-page' as const;
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

      /**
       * List of unqualified components from rmad service, not i18n.
       */
      componentsList: {
        type: String,
        value: '',
      },

      verificationInProgress: {
        type: Boolean,
        value: true,
      },

      /**
       * isCompliant is not valid until verificationInProgress is false.
       */
      isCompliant: {
        type: Boolean,
        value: false,
      },

      /**
       * isSkipped is not valid until verificationInProgress is false.
       */
      isSkipped: {
        type: Boolean,
        value: false,
      },

      /**
       * After the Get Started button is clicked, true until the next state is
       * processed. It is set back to false by shimless_rma.ts.
       */
      getStartedButtonClicked: {
        type: Boolean,
        value: false,
      },

      /**
       * After the exit button is clicked, true until the next state is
       * processed. It is set back to false by shimless_rma.ts.
       */
      confirmExitButtonClicked: {
        type: Boolean,
        value: false,
      },

      /**
       * Hide the exit button if user should not exit.
       */
      canExit: {
        type: Boolean,
        value: true,
      },

      verificationFailedMessage: {
        type: String,
        value: '',
      },
    };
  }

  allButtonsDisabled: boolean;
  getStartedButtonClicked: boolean;
  confirmExitButtonClicked: boolean;
  canExit: boolean;
  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  hwVerificationObserverReceiver: HardwareVerificationStatusObserverReceiver;
  protected componentsList: string;
  protected verificationInProgress: boolean;
  protected isCompliant: boolean;
  protected isSkipped: boolean;
  protected verificationFailedMessage: TrustedHTML;

  constructor() {
    super();
    this.hwVerificationObserverReceiver =
        new HardwareVerificationStatusObserverReceiver(this);

    this.shimlessRmaService.observeHardwareVerificationStatus(
        this.hwVerificationObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    if (this.verificationInProgress) {
      return Promise.reject(new Error('Hardware verification is not complete.'));
    }

    return this.shimlessRmaService.beginFinalization();
  }

  protected onGetStartedButtonClicked(e: Event): void {
    e.preventDefault();

    this.getStartedButtonClicked = true;

    executeThenTransitionState(this, () => {
      if (this.verificationInProgress) {
        return Promise.reject(
          new Error('Hardware verification is not complete.'));
      }

      return this.shimlessRmaService.beginFinalization();
    });
  }

  protected onLandingExitButtonClicked(e: Event): void {
    e.preventDefault();
    this.dispatchEvent(createCustomEvent(CLICK_EXIT_BUTTON, {}));
  }

  protected getVerificationIcon(): string {
    return this.isCompliant ? 'shimless-icon:check' : 'shimless-icon:warning';
  }

  /**
   * Implements
   * HardwareVerificationStatusObserver.onHardwareVerificationResult()
   */
  onHardwareVerificationResult(result: HardwareVerificationResult): void {
    this.isCompliant = result.passResult !== undefined;
    this.isSkipped = loadTimeData.getBoolean('hardwareValidationSkipEnabled') &&
        result.skipResult !== undefined;
    this.verificationInProgress = false;

    if (!this.isSkipped && !this.isCompliant) {
      this.componentsList = result.failResult!.componentInfo;
      this.setVerificationFailedMessage();
    }
  }

  private setVerificationFailedMessage(): void {
    this.verificationFailedMessage =
        this.i18nAdvanced('validatedComponentsFailText', {attrs: ['id']});
    const linkElement =
        this.shadowRoot!.querySelector('#unqualifiedComponentsLink');
    assert(linkElement);
    linkElement.setAttribute('href', '#');
    const dialog: CrDialogElement|null =
      this.shadowRoot!.querySelector('#unqualifiedComponentsDialog');
    assert(dialog);
    linkElement.addEventListener('click', () => dialog.showModal());
  }

  private closeDialog(): void {
    const dialog: CrDialogElement|null =
      this.shadowRoot!.querySelector('#unqualifiedComponentsDialog');
    assert(dialog);
    dialog.close();
  }

  protected isGetStartedButtonDisabled(): boolean {
    return this.verificationInProgress || this.allButtonsDisabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingLandingPage.is]: OnboardingLandingPage;
  }
}

customElements.define(OnboardingLandingPage.is, OnboardingLandingPage);
