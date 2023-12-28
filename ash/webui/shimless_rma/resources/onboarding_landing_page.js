// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_landing_page.html.js';
import {HardwareVerificationStatusObserverInterface, HardwareVerificationStatusObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-landing-page' is the main landing page for the shimless rma
 * process.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingLandingPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingLandingPage extends OnboardingLandingPageBase {
  static get is() {
    return 'onboarding-landing-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

      /**
       * List of unqualified components from rmad service, not i18n.
       * @protected
       */
      componentsList: {
        type: String,
        value: '',
      },

      /** @protected */
      verificationInProgress: {
        type: Boolean,
        value: true,
      },

      /**
       * isCompliant is not valid until verificationInProgress is false.
       * @protected
       */
      isCompliant: {
        type: Boolean,
        value: false,
      },

      /**
       * After the Get Started button is clicked, true until the next state is
       * processed. It is set back to false by shimless_rma.js.
       */
      getStartedButtonClicked: {
        type: Boolean,
        value: false,
      },

      /**
       * After the exit button is clicked, true until the next state is
       * processed. It is set back to false by shimless_rma.js.
       */
      confirmExitButtonClicked: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      verificationFailedMessage: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
    /** @protected {?HardwareVerificationStatusObserverReceiver} */
    this.hwVerificationObserverReceiver =
        new HardwareVerificationStatusObserverReceiver(
            /**
             * @type {!HardwareVerificationStatusObserverInterface}
             */
            (this));

    this.shimlessRmaService.observeHardwareVerificationStatus(
        this.hwVerificationObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /** @return {!Promise<{stateResult: !StateResult}>} */
  onNextButtonClick() {
    if (!this.verificationInProgress) {
      return this.shimlessRmaService.beginFinalization();
    }

    return Promise.reject(new Error('Hardware verification is not complete.'));
  }

  /** @protected */
  onGetStartedButtonClicked(e) {
    e.preventDefault();

    this.getStartedButtonClicked = true;

    executeThenTransitionState(this, () => {
      if (!this.verificationInProgress) {
        return this.shimlessRmaService.beginFinalization();
      }

      return Promise.reject(
          new Error('Hardware verification is not complete.'));
    });
  }

  /**
   * @protected
   */
  onLandingExitButtonClicked(e) {
    e.preventDefault();

    this.dispatchEvent(new CustomEvent(
        'click-exit-button',
        {
          bubbles: true,
          composed: true,
        },
        ));
  }

  /**
   * @return {string}
   * @protected
   */
  getVerificationIcon() {
    return this.isCompliant ? 'shimless-icon:check' : 'shimless-icon:warning';
  }

  /**
   * Implements
   * HardwareVerificationStatusObserver.onHardwareVerificationResult()
   * @param {boolean} isCompliant
   * @param {string} errorMessage
   */
  onHardwareVerificationResult(isCompliant, errorMessage) {
    this.isCompliant = isCompliant;
    this.verificationInProgress = false;

    if (!this.isCompliant) {
      this.componentsList = errorMessage;
      this.setVerificationFailedMessage();
    }
  }

  /** @private */
  setVerificationFailedMessage() {
    this.verificationFailedMessage =
        this.i18nAdvanced('validatedComponentsFailText', {attrs: ['id']});
    const linkElement =
        this.shadowRoot.querySelector('#unqualifiedComponentsLink');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener(
        'click',
        () => this.shadowRoot.querySelector('#unqualifiedComponentsDialog')
                  .showModal());
  }

  /** @private */
  closeDialog() {
    this.shadowRoot.querySelector('#unqualifiedComponentsDialog').close();
  }

  /** @protected */
  isGetStartedButtonDisabled() {
    return this.verificationInProgress || this.allButtonsDisabled;
  }
}

customElements.define(OnboardingLandingPage.is, OnboardingLandingPage);
