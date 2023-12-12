// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_enter_rsu_wp_disable_code_page.html.js';
import {QrCode, RmadErrorCode, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {dispatchNextButtonClick, enableNextButton} from './shimless_rma_util.js';

// The number of characters in an RSU code.
const RSU_CODE_EXPECTED_LENGTH = 8;

/**
 * @fileoverview
 * 'onboarding-enter-rsu-wp-disable-code-page' asks the user for the RSU disable
 * code.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingEnterRsuWpDisableCodePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingEnterRsuWpDisableCodePage extends
    OnboardingEnterRsuWpDisableCodePageBase {
  static get is() {
    return 'onboarding-enter-rsu-wp-disable-code-page';
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
       * Set by shimless_rma.js.
       * @type {RmadErrorCode}
       */
      errorCode: {
        type: Object,
        observer:
            OnboardingEnterRsuWpDisableCodePage.prototype.onErrorCodeChanged,
      },

      /** @protected */
      canvasSize: {
        type: Number,
        value: 0,
      },

      /** @protected {string} */
      rsuChallenge: {
        type: String,
        value: '',
      },

      /** @protected */
      rsuHwid: {
        type: String,
        value: '',
      },

      /** @protected */
      rsuCode: {
        type: String,
        value: '',
        observer:
            OnboardingEnterRsuWpDisableCodePage.prototype.onRsuCodeChanged,
      },

      /** @protected */
      rsuCodeExpectedLength: {
        type: Number,
        value: RSU_CODE_EXPECTED_LENGTH,
        readOnly: true,
      },

      /** @protected */
      rsuInstructionsText: {
        type: String,
        value: '',
      },

      /** @protected */
      qrCodeUrl: {
        type: String,
        value: '',
      },

      /** @protected */
      rsuChallengeLinkText: {
        type: String,
        value: '',
        computed: 'computeRsuChallengeLinkText(rsuHwid, rsuChallenge)',
      },

      /** @protected */
      rsuCodeValidationRegex: {
        type: String,
        value: '.{1,8}',
        readOnly: true,
      },

      /** @protected {boolean} */
      rsuCodeInvalid: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();
    this.getRsuChallengeAndHwid();
    this.setRsuInstructionsText();
    enableNextButton(this);

    afterNextRender(this, () => {
      const codeInput = this.shadowRoot.querySelector('#rsuCode');
      codeInput.focus();
    });
  }

  /** @private */
  getRsuChallengeAndHwid() {
    this.shimlessRmaService.getRsuDisableWriteProtectChallenge().then(
        (result) => this.rsuChallenge = result.challenge);
    this.shimlessRmaService.getRsuDisableWriteProtectHwid().then((result) => {
      this.rsuHwid = result.hwid;
    });
    this.shimlessRmaService.getRsuDisableWriteProtectChallengeQrCode().then(
        this.updateQrCode.bind(this));
  }

  /**
   * @param {{qrCodeData: !Array<number>}} response
   * @private
   */
  updateQrCode(response) {
    const blob =
        new Blob([Uint8Array.from(response.qrCodeData)], {'type': 'image/png'});
    this.qrCodeUrl = URL.createObjectURL(blob);
  }

  /**
   * @return {boolean}
   * @private
   */
  rsuCodeIsPlausible() {
    return !!this.rsuCode && this.rsuCode.length === RSU_CODE_EXPECTED_LENGTH;
  }

  /**
   * @param {!Event} event
   * @protected
   */
  onRsuCodeChanged(event) {
    // Set to false whenever the user changes the code to remove the red invalid
    // warning.
    this.rsuCodeInvalid = false;
    this.rsuCode = this.rsuCode.toUpperCase();
  }

  /**
   * @param {!Event} event
   * @protected
   */
  onKeyDown(event) {
    if (event.key === 'Enter') {
      dispatchNextButtonClick(this);
    }
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    if (this.rsuCode.length !== this.rsuCodeExpectedLength) {
      this.rsuCodeInvalid = true;
      return Promise.reject(new Error('No RSU code set'));
    }

    return this.shimlessRmaService.setRsuDisableWriteProtectCode(this.rsuCode);
  }

  /** @private */
  setRsuInstructionsText() {
    this.rsuInstructionsText =
        this.i18nAdvanced('rsuCodeInstructionsText', {attrs: ['id']});
    const linkElement = this.shadowRoot.querySelector('#rsuCodeDialogLink');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => {
      if (this.allButtonsDisabled) {
        return;
      }

      this.shadowRoot.querySelector('#rsuChallengeDialog').showModal();
    });
  }

  /**
   * @return {string}
   * @private
   */
  computeRsuChallengeLinkText() {
    const unlockPageUrl =
        'https://chromeos.google.com/partner/console/cr50reset?challenge=';
    return unlockPageUrl + this.rsuChallenge + '&hwid=' + this.rsuHwid;
  }

  /** @private */
  closeDialog() {
    this.shadowRoot.querySelector('#rsuChallengeDialog').close();
  }

  /** @private */
  onErrorCodeChanged() {
    if (this.errorCode === RmadErrorCode.kWriteProtectDisableRsuCodeInvalid) {
      this.rsuCodeInvalid = true;
    }
  }

  /**
   * @return {string}
   * @protected
   */
  getRsuCodeLabelText() {
    return this.rsuCodeInvalid ? this.i18n('rsuCodeErrorLabelText') :
                                 this.i18n('rsuCodeLabelText');
  }

  /**
   * @return {string}
   * @protected
   */
  getRsuAriaDescription() {
    return `${this.getRsuCodeLabelText()} ${
        this.i18n('rsuCodeInstructionsAriaText')}`;
  }
}

customElements.define(
    OnboardingEnterRsuWpDisableCodePage.is,
    OnboardingEnterRsuWpDisableCodePage);
