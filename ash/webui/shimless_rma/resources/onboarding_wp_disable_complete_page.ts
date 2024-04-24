// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_wp_disable_complete_page.html.js';
import {ShimlessRmaServiceInterface, StateResult, WriteProtectDisableCompleteAction} from './shimless_rma.mojom-webui.js';
import {enableNextButton, focusPageTitle} from './shimless_rma_util.js';

type DisableActionTextKeys = {
  [key in WriteProtectDisableCompleteAction]: string;
};

const disableActionTextKeys: DisableActionTextKeys = {
  [WriteProtectDisableCompleteAction.kUnknown]: '',
  [WriteProtectDisableCompleteAction.kSkippedAssembleDevice]:
      'wpDisableReassembleNowText',
  [WriteProtectDisableCompleteAction.kCompleteAssembleDevice]:
      'wpDisableReassembleNowText',
  [WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen]:
      'wpDisableLeaveDisassembledText',
  [WriteProtectDisableCompleteAction.kCompleteNoOp]: '',
};

/**
 * @fileoverview
 * 'onboarding-wp-disable-complete-page' notifies the user that manual HWWP
 * disable was successful, and what steps must be taken next.
 */

const OnboardingWpDisableCompletePageBase = I18nMixin(PolymerElement);

export class OnboardingWpDisableCompletePage extends
    OnboardingWpDisableCompletePageBase {
  static get is() {
    return 'onboarding-wp-disable-complete-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      actionString: {
        type: String,
        computed: 'getActionString(action)',
      },
    };
  }

  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  protected actionString: string;
  private action: WriteProtectDisableCompleteAction = WriteProtectDisableCompleteAction.kUnknown;

  override ready() {
    super.ready();
    enableNextButton(this);

    focusPageTitle(this);
  }

  constructor() {
    super();

    this.shimlessRmaService.getWriteProtectDisableCompleteAction().then(
        (res: {action: WriteProtectDisableCompleteAction }) => {
          if (res) {
            this.action = res.action;
          }
        });
  }

  protected getActionString(): string {
    return (this.action === WriteProtectDisableCompleteAction.kUnknown ||
            this.action === WriteProtectDisableCompleteAction.kCompleteNoOp) ?
        '' :
        this.i18n(disableActionTextKeys[this.action]);
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    return this.shimlessRmaService.confirmManualWpDisableComplete();
  }

  protected getVerificationIcon(): string {
    return (this.action === WriteProtectDisableCompleteAction.kUnknown ||
            this.action === WriteProtectDisableCompleteAction.kCompleteNoOp) ?
        '' :
        'shimless-icon:check';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingWpDisableCompletePage.is]: OnboardingWpDisableCompletePage;
  }
}

customElements.define(
    OnboardingWpDisableCompletePage.is, OnboardingWpDisableCompletePage);
