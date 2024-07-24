// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-ad-topics-fragment' is the fragment in a privacy guide
 * card that contains the ad topics setting and its description.
 */

import '../../controls/settings_toggle_button.js';
import '../../icons.html.js';
import './privacy_guide_description_item.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';
import {PrivacySandboxBrowserProxyImpl} from '../../privacy_sandbox/privacy_sandbox_browser_proxy.js';

import {getTemplate} from './privacy_guide_ad_topics_fragment.html.js';

const PrivacyGuideAdTopicsFragmentElementBase = PrefsMixin(PolymerElement);

export class PrivacyGuideAdTopicsFragmentElement extends
    PrivacyGuideAdTopicsFragmentElementBase {
  static get is() {
    return 'privacy-guide-ad-topics-fragment';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  private startStateAdTopicsOn_: boolean;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  override focus() {
    // The fragment element is focused when it becomes visible. Move the focus
    // to the fragment header, so that the newly shown content of the fragment
    // is downwards from the focus position. This allows users of screen readers
    // to continue navigating the screen reader position downwards through the
    // newly visible content.
    this.shadowRoot!.querySelector<HTMLElement>('[focus-element]')!.focus();
  }

  private onViewEnterStart_() {
    this.startStateAdTopicsOn_ =
        this.getPref<boolean>('privacy_sandbox.m1.topics_enabled').value;
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.AD_TOPICS_REACHED);
  }

  private onViewExitFinish_() {
    const endStateAdTopicsOn =
        this.getPref<boolean>('privacy_sandbox.m1.topics_enabled').value;

    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateAdTopicsOn_) {
      state = endStateAdTopicsOn ?
          PrivacyGuideSettingsStates.AD_TOPICS_ON_TO_ON :
          PrivacyGuideSettingsStates.AD_TOPICS_ON_TO_OFF;
    } else {
      state = endStateAdTopicsOn ?
          PrivacyGuideSettingsStates.AD_TOPICS_OFF_TO_ON :
          PrivacyGuideSettingsStates.AD_TOPICS_OFF_TO_OFF;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state!);
  }

  private onToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    PrivacySandboxBrowserProxyImpl.getInstance().topicsToggleChanged(
        target.checked);
    this.metricsBrowserProxy_.recordAction(
        target.checked ? 'Settings.PrivacyGuide.ChangeAdTopicsOn' :
                         'Settings.PrivacyGuide.ChangeAdTopicsOff');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-ad-topics-fragment': PrivacyGuideAdTopicsFragmentElement;
  }
}

customElements.define(
    PrivacyGuideAdTopicsFragmentElement.is,
    PrivacyGuideAdTopicsFragmentElement);
