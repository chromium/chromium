// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_enter_rsu_wp_disable_code_page.html.js';
import {RmadErrorCode, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {dispatchNextButtonClick, enableNextButton} from './shimless_rma_util.js';

// The number of characters in an RSU code.
const RSU_CODE_EXPECTED_LENGTH = 8;

/**
 * @fileoverview
 * 'onboarding-enter-rsu-wp-disable-code-page' asks the user for the RSU disable
 * code.
 */

const OnboardingEnterRsuWpDisableCodePageBase = I18nMixin(PolymerElement);

export class OnboardingEnterRsuWpDisableCodePage extends
    OnboardingEnterRsuWpDisableCodePageBase {
  static get is() {
    return 'onboarding-enter-rsu-wp-disable-code-page' as const;
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
       * Set by shimless_rma.ts.
       */
      errorCode: {
        type: Object,
        observer:
            OnboardingEnterRsuWpDisableCodePage.prototype.onErrorCodeChanged,
      },

      canvasSize: {
        type: Number,
        value: 0,
      },

      rsuChallenge: {
        type: String,
        value: '',
      },

      rsuHwid: {
        type: String,
        value: '',
      },

      rsuCode: {
        type: String,
        value: '',
        observer:
            OnboardingEnterRsuWpDisableCodePage.prototype.onRsuCodeChanged,
      },

      rsuCodeExpectedLength: {
        type: Number,
        value: RSU_CODE_EXPECTED_LENGTH,
        readOnly: true,
      },

      rsuInstructionsText: {
        type: String,
        value: '',
      },

      qrCodeUrl: {
        type: String,
        value: '',
      },

      rsuChallengeLinkText: {
        type: String,
        value: '',
        computed: 'computeRsuChallengeLinkText(rsuHwid, rsuChallenge)',
      },

      rsuCodeValidationRegex: {
        type: String,
        value: '.{1,8}',
        readOnly: true,
      },

      rsuCodeInvalid: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  allButtonsDisabled: boolean;
  errorCode: RmadErrorCode;
  protected canvasSize: number;
  protected rsuChallenge: string;
  protected rsuHwid: string;
  protected rsuCode: string;
  protected rsuCodeExpectedLength: number;
  protected rsuInstructionsText: TrustedHTML;
  protected qrCodeUrl: string;
  protected rsuChallengeLinkText: string;
  protected rsuCodeValidationRegex: string;
  protected rsuCodeInvalid: boolean;

  override ready() {
    super.ready();
    this.getRsuChallengeAndHwid();
    this.setRsuInstructionsText();
    enableNextButton(this);

    afterNextRender(this, () => {
      const codeInput: CrInputElement|null = this.shadowRoot!.querySelector('#rsuCode');
      assert(codeInput);
      codeInput.focus();
    });
  }

  private getRsuChallengeAndHwid(): void {
    this.shimlessRmaService.getRsuDisableWriteProtectChallenge().then(
        (result: {challenge: string}) => this.rsuChallenge = result.challenge);
    this.shimlessRmaService.getRsuDisableWriteProtectHwid().then((result: {hwid: string}) => {
      this.rsuHwid = result.hwid;
    });
    this.shimlessRmaService.getRsuDisableWriteProtectChallengeQrCode().then(
        this.updateQrCode.bind(this));
  }

  private updateQrCode(response: {qrCodeData: number[]}): void {
    const blob =
        new Blob([Uint8Array.from(response.qrCodeData)], {'type': 'image/png'});
    this.qrCodeUrl = URL.createObjectURL(blob);
  }

  private rsuCodeIsPlausible(): boolean {
    return !!this.rsuCode && this.rsuCode.length === RSU_CODE_EXPECTED_LENGTH;
  }

  protected onRsuCodeChanged(): void {
    // Set to false whenever the user changes the code to remove the red invalid
    // warning.
    this.rsuCodeInvalid = false;
    this.rsuCode = this.rsuCode.toUpperCase();
  }

  protected onKeyDown(event: KeyboardEvent): void {
    if (event.key === 'Enter') {
      dispatchNextButtonClick(this);
    }
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    if (this.rsuCode.length !== this.rsuCodeExpectedLength) {
      this.rsuCodeInvalid = true;
      return Promise.reject(new Error('No RSU code set'));
    }

    return this.shimlessRmaService.setRsuDisableWriteProtectCode(this.rsuCode);
  }

  private setRsuInstructionsText(): void {
    this.rsuInstructionsText =
        this.i18nAdvanced('rsuCodeInstructionsText', {attrs: ['id']});
    const linkElement: HTMLAnchorElement|null = this.shadowRoot!.querySelector('#rsuCodeDialogLink');
    assert(linkElement);
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => {
      if (this.allButtonsDisabled) {
        return;
      }

      const dialog: CrDialogElement|null = this.shadowRoot!.querySelector('#rsuChallengeDialog');
      assert(dialog);
      dialog.showModal();
    });
  }

  private computeRsuChallengeLinkText(): string {
    const unlockPageUrl =
        'https://chromeos.google.com/partner/console/cr50reset?challenge=';
    return unlockPageUrl + this.rsuChallenge + '&hwid=' + this.rsuHwid;
  }

  private closeDialog(): void {
    const dialog: CrDialogElement|null = this.shadowRoot!.querySelector('#rsuChallengeDialog');
    assert(dialog);
    dialog.close();
  }

  private onErrorCodeChanged(): void {
    if (this.errorCode === RmadErrorCode.kWriteProtectDisableRsuCodeInvalid) {
      this.rsuCodeInvalid = true;
    }
  }

  protected getRsuCodeLabelText(): string {
    return this.rsuCodeInvalid ? this.i18n('rsuCodeErrorLabelText') :
                                 this.i18n('rsuCodeLabelText');
  }

  protected getRsuAriaDescription(): string {
    return `${this.getRsuCodeLabelText()} ${
        this.i18n('rsuCodeInstructionsAriaText')}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingEnterRsuWpDisableCodePage.is]: OnboardingEnterRsuWpDisableCodePage;
  }
}

customElements.define(
    OnboardingEnterRsuWpDisableCodePage.is,
    OnboardingEnterRsuWpDisableCodePage);
