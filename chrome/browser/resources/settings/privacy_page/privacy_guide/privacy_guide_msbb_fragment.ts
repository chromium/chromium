// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-msbb-fragment' is the fragment in a privacy guide card
 * that contains the MSBB setting with a two-column description.
 */
import '/shared/settings/prefs/prefs.js';
import '../../controls/settings_toggle_button.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';

import {getTemplate} from './privacy_guide_msbb_fragment.html.js';

const PrivacyGuideMsbbFragmentBase = PrefsMixin(PolymerElement);

export class PrivacyGuideMsbbFragmentElement extends
    PrivacyGuideMsbbFragmentBase {
  static get is() {
    return 'privacy-guide-msbb-fragment';
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

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private startStateMsbbOn_: boolean;

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
    this.startStateMsbbOn_ =
        this.getPref<boolean>('url_keyed_anonymized_data_collection.enabled')
            .value;
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.MSBB_REACHED);
  }

  private onViewExitFinish_() {
    const endStateMsbbOn =
        this.getPref<boolean>('url_keyed_anonymized_data_collection.enabled')
            .value;

    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateMsbbOn_) {
      state = endStateMsbbOn ? PrivacyGuideSettingsStates.MSBB_ON_TO_ON :
                               PrivacyGuideSettingsStates.MSBB_ON_TO_OFF;
    } else {
      state = endStateMsbbOn ? PrivacyGuideSettingsStates.MSBB_OFF_TO_ON :
                               PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state!);
  }

  private onMsbbToggleClick_() {
    if (this.getPref('url_keyed_anonymized_data_collection.enabled').value) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacyGuide.ChangeMSBBOn');
    } else {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacyGuide.ChangeMSBBOff');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-msbb-fragment': PrivacyGuideMsbbFragmentElement;
  }
}

customElements.define(
    PrivacyGuideMsbbFragmentElement.is, PrivacyGuideMsbbFragmentElement);
