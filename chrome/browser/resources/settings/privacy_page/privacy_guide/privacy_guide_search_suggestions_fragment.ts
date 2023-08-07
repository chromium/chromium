// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-search-suggestions-fragment' is the fragment in a privacy
 * guide card that contains the search suggestions setting with a two-column
 * description.
 */
import '/shared/settings/controls/settings_toggle_button.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';

import {getTemplate} from './privacy_guide_search_suggestions_fragment.html.js';

const PrivacyGuideMsbbFragmentBase = PrefsMixin(PolymerElement);

export class PrivacyGuideSearchSuggestionsFragmentElement extends
    PrivacyGuideMsbbFragmentBase {
  static get is() {
    return 'privacy-guide-search-suggestions-fragment';
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
  private startStateSearchSuggestionsOn_: boolean;

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
    this.startStateSearchSuggestionsOn_ =
        this.getPref<boolean>('search.suggest_enabled').value;
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.SEARCH_SUGGESTIONS_REACHED);
  }

  private onViewExitFinish_() {
    const endStateSearchSuggestionsOn =
        this.getPref<boolean>('search.suggest_enabled').value;

    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateSearchSuggestionsOn_) {
      state = endStateSearchSuggestionsOn ?
          PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_ON :
          PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_OFF;
    } else {
      state = endStateSearchSuggestionsOn ?
          PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_ON :
          PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_OFF;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state!);
  }

  private onSearchSuggestionsToggleClick_() {
    if (this.getPref('search.suggest_enabled').value) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacyGuide.ChangeSearchSuggestionsOn');
    } else {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacyGuide.ChangeSearchSuggestionsOff');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-search-suggestions-fragment':
        PrivacyGuideSearchSuggestionsFragmentElement;
  }
}

customElements.define(
    PrivacyGuideSearchSuggestionsFragmentElement.is,
    PrivacyGuideSearchSuggestionsFragmentElement);
