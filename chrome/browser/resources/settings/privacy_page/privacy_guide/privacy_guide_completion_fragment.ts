// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-completion-fragment' is the fragment in a privacy guide
 * card that contains the completion screen and its description.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../icons.html.js';
import '../../privacy_icons.html.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {UpdateSyncStateEvent} from '../../clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
import {ClearBrowsingDataBrowserProxyImpl} from '../../clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';

import {getCss} from './privacy_guide_completion_fragment.css.js';
import {getHtml} from './privacy_guide_completion_fragment.html.js';

export interface PrivacyGuideCompletionFragmentElement {
  $: {
    backButton: HTMLElement,
  };
}

const PrivacyGuideCompletionFragmentElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class PrivacyGuideCompletionFragmentElement extends
    PrivacyGuideCompletionFragmentElementBase {
  static get is() {
    return 'privacy-guide-completion-fragment';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shouldShowAiSettings_: {type: Boolean},
      shouldShowWaa_: {type: Boolean},
    };
  }

  protected accessor shouldShowAiSettings_: boolean =
      loadTimeData.getBoolean('showAiPage');
  protected accessor shouldShowWaa_: boolean = false;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);

    this.addWebUiListener(
        'update-sync-state',
        (event: UpdateSyncStateEvent) => this.updateWaaLink_(event.signedIn));
    ClearBrowsingDataBrowserProxyImpl.getInstance().getSyncState().then(
        (status: UpdateSyncStateEvent) => this.updateWaaLink_(status.signedIn));
  }

  override focus() {
    const header = this.shadowRoot.querySelector<HTMLElement>(
        '.welcome-completion-header-label');
    assert(header);
    header.focus();
  }

  private onViewEnterStart_() {
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE);
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.COMPLETION_REACHED);
  }

  protected getSubheader_(): string {
    return !this.shouldShowWaa_ ?
        this.i18n('privacyGuideCompletionCardSubHeaderNoLinks') :
        this.i18n('privacyGuideCompletionCardSubHeader');
  }

  /** Updates the completion card waa link depending on the signin state. */
  private updateWaaLink_(isSignedIn: boolean) {
    this.shouldShowWaa_ = isSignedIn;
  }

  protected onBackButtonClick_(e: Event) {
    e.stopPropagation();
    this.fire('back-button-click');
  }

  protected onLeaveButtonClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
        PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.NextClickCompletion');
    // Send a |close| event to the privacy guide dialog to close itself.
    this.fire('close');
  }

  protected onAiRowClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.AI_SETTINGS_COMPLETION_LINK);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.CompletionAiSettingsClick');
    // TODO(crbug.com/40162029): Replace this with an ordinary OpenWindowProxy
    // call.
    this.shadowRoot.querySelector<HTMLAnchorElement>(
                       '#aiRowLink')!.dispatchEvent(new MouseEvent('click'));
  }

  protected onWaaClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.SWAA_COMPLETION_LINK);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.CompletionSWAAClick');
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('activityControlsUrlInPrivacyGuide'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-completion-fragment': PrivacyGuideCompletionFragmentElement;
  }
}

customElements.define(
    PrivacyGuideCompletionFragmentElement.is,
    PrivacyGuideCompletionFragmentElement);
