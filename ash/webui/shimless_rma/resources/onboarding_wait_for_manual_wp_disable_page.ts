// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_wait_for_manual_wp_disable_page.html.js';
import {HardwareWriteProtectionStateObserverReceiver, ShimlessRmaServiceInterface} from './shimless_rma.mojom-webui.js';
import {disableAllButtons, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-wait-for-manual-wp-disable-page' wait for the manual HWWP disable
 * to be completed.
 */

const OnboardingWaitForManualWpDisablePageBase = I18nMixin(PolymerElement);

export class OnboardingWaitForManualWpDisablePage extends
    OnboardingWaitForManualWpDisablePageBase {
  static get is() {
    return 'onboarding-wait-for-manual-wp-disable-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hwwpEnabled: {
        type: Boolean,
        value: true,
      },
    };
  }

  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  private hardwareWriteProtectionStateObserverReceiver: HardwareWriteProtectionStateObserverReceiver;
  protected hwwpEnabled: boolean;

  constructor() {
    super();
    this.hardwareWriteProtectionStateObserverReceiver =
        new HardwareWriteProtectionStateObserverReceiver(this);

    this.shimlessRmaService.observeHardwareWriteProtectionState(
        this.hardwareWriteProtectionStateObserverReceiver.$
            .bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  onHardwareWriteProtectionStateChanged(enabled: boolean) {
    this.hwwpEnabled = enabled;

    if(!this.hidden) {
      if (!this.hwwpEnabled) {
        disableAllButtons(this, /*showBusyStateOverlay=*/ false);
      }
    }
  }

  protected getPageTitle(): string {
    return this.hwwpEnabled ? this.i18n('manuallyDisableWpTitleText') :
                              this.i18n('manuallyDisableWpTitleTextReboot');
  }

  protected getInstructions(): string {
    return this.hwwpEnabled ?
        this.i18n('manuallyDisableWpInstructionsText') :
        this.i18n('manuallyDisableWpInstructionsTextReboot');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingWaitForManualWpDisablePage.is]: OnboardingWaitForManualWpDisablePage;
  }
}

customElements.define(
    OnboardingWaitForManualWpDisablePage.is,
    OnboardingWaitForManualWpDisablePage);
