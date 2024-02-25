// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './icons.html.js';
import './shimless_rma_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_landing_page.html.js';
import {HardwareVerificationStatusObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';
import {ClickExitButtonEvent, CLICK_EXIT_BUTTON, createCustomEvent} from './events.js';

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

      verificationFailedMessage: {
        type: String,
        value: '',
      },
    };
  }

  allButtonsDisabled: boolean;
  getStartedButtonClicked: boolean;
  confirmExitButtonClicked: boolean;
  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  hwVerificationObserverReceiver: HardwareVerificationStatusObserverReceiver;
  protected componentsList: string;
  protected verificationInProgress: boolean;
  protected isCompliant: boolean;
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
  onHardwareVerificationResult(isCompliant: boolean, errorMessage: string): void {
    this.isCompliant = isCompliant;
    this.verificationInProgress = false;

    if (!this.isCompliant) {
      this.componentsList = errorMessage;
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
