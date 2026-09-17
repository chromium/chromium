// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-safe-browsing-fragment' is the fragment in a privacy
 * guide card that contains the safe browsing settings and their descriptions.
 */

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../../controls/collapse_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../icons.html.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';
import {SafeBrowsingSetting} from '../security/safe_browsing_types.js';

import {getCss as getPrivacyGuideFragmentSharedCss} from './privacy_guide_fragment_shared_lit.css.js';
import {getHtml} from './privacy_guide_safe_browsing_fragment.html.js';

const PrivacyGuideSafeBrowsingFragmentElementBase = I18nMixinLit(CrLitElement);

export class PrivacyGuideSafeBrowsingFragmentElement extends
    PrivacyGuideSafeBrowsingFragmentElementBase {
  static get is() {
    return 'privacy-guide-safe-browsing-fragment';
  }

  static override get styles() {
    return [
      getPrivacyGuideFragmentSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private startStateEnhanced_: boolean;
  private enableHashPrefixRealTimeLookups_: boolean =
      loadTimeData.getBoolean('enableHashPrefixRealTimeLookups');

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  override focus() {
    // The fragment element is focused when it becomes visible. Move the focus
    // to the fragment header, so that the newly shown content of the fragment
    // is downwards from the focus position. This allows users of screen readers
    // to continue navigating the screen reader position downwards through the
    // newly visible content.
    this.shadowRoot.querySelector<HTMLElement>('[focus-element]')!.focus();
  }

  private onViewEnterStart_() {
    this.startStateEnhanced_ = PrefService.getInstance()
                                   .getPref<number>('generated.safe_browsing')
                                   .value === SafeBrowsingSetting.ENHANCED;
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.SAFE_BROWSING_REACHED);
  }

  private onViewExitFinish_() {
    const endStateEnhanced = PrefService.getInstance()
                                 .getPref<number>('generated.safe_browsing')
                                 .value === SafeBrowsingSetting.ENHANCED;

    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateEnhanced_) {
      state = endStateEnhanced ?
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED :
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD;
    } else {
      state = endStateEnhanced ?
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED :
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state);
  }

  protected onSafeBrowsingEnhancedClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced');
  }

  protected onSafeBrowsingStandardClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.ChangeSafeBrowsingStandard');
  }

  protected getSafeBrowsingStandardSubLabel_(): string {
    return this.i18n(
        this.enableHashPrefixRealTimeLookups_ ?
            'safeBrowsingStandardDescProxy' :
            'safeBrowsingStandardDesc');
  }

  private getStandardProtectionFeatureDescription2_(): string {
    return this.i18n(
        this.enableHashPrefixRealTimeLookups_ ?
            'privacyGuideSafeBrowsingCardStandardProtectionFeatureDescription2Proxy' :
            'privacyGuideSafeBrowsingCardStandardProtectionFeatureDescription2');
  }

  private getStandardProtectionPrivacyDescription1_(): string {
    return this.i18n(
        this.enableHashPrefixRealTimeLookups_ ?
            'privacyGuideSafeBrowsingCardStandardProtectionPrivacyDescription1Proxy' :
            'privacyGuideSafeBrowsingCardStandardProtectionPrivacyDescription1');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-safe-browsing-fragment':
        PrivacyGuideSafeBrowsingFragmentElement;
  }
}

customElements.define(
    PrivacyGuideSafeBrowsingFragmentElement.is,
    PrivacyGuideSafeBrowsingFragmentElement);
