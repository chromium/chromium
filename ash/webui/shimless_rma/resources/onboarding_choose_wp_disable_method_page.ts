// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_choose_wp_disable_method_page.html.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {disableNextButton, enableNextButton, focusPageTitle} from './shimless_rma_util.js';
import {OnSelectedChangedEvent} from './events.js';

/**
 * @fileoverview
 * 'onboarding-choose-wp-disable-method-page' allows user to select between
 * hardware or RSU write protection disable methods.
 *
 * TODO(gavindodd): Change "Manual" description based on enterprise enrollment
 * status.
 */

const OnboardingChooseWpDisableMethodPageBase = I18nMixin(PolymerElement);

export class OnboardingChooseWpDisableMethodPage extends
  OnboardingChooseWpDisableMethodPageBase {
  static get is() {
    return 'onboarding-choose-wp-disable-method-page' as const;
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

      hwwpMethod: {
        type: String,
        value: '',
      },
    };
  }

  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  allButtonsDisabled: boolean;
  private hwwpMethod: string;

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  protected onHwwpDisableMethodSelectionChanged(event: OnSelectedChangedEvent) {
    this.hwwpMethod = event.detail.value;
    const disabled = !this.hwwpMethod;
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    if (this.hwwpMethod === 'hwwpDisableMethodManual') {
      return this.shimlessRmaService.setManuallyDisableWriteProtect();
    } else if (this.hwwpMethod === 'hwwpDisableMethodRsu') {
      return this.shimlessRmaService.setRsuDisableWriteProtect();
    } else {
      return Promise.reject(new Error('No disable method selected'));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingChooseWpDisableMethodPage.is]: OnboardingChooseWpDisableMethodPage;
  }
}

customElements.define(
  OnboardingChooseWpDisableMethodPage.is,
  OnboardingChooseWpDisableMethodPage);
