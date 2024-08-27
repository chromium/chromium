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
import './privacy_guide_fragment_shared.css.js';
import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {UpdateSyncStateEvent} from '../../clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
import {ClearBrowsingDataBrowserProxyImpl} from '../../clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../../hats_browser_proxy.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';

import type {PrivacyGuideBrowserProxy} from './privacy_guide_browser_proxy.js';
import {PrivacyGuideBrowserProxyImpl} from './privacy_guide_browser_proxy.js';
import {getTemplate} from './privacy_guide_completion_fragment.html.js';

export interface PrivacyGuideCompletionFragmentElement {
  $: {
    backButton: HTMLElement,
  };
}

const PrivacyGuideCompletionFragmentElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class PrivacyGuideCompletionFragmentElement extends
    PrivacyGuideCompletionFragmentElementBase {
  static get is() {
    return 'privacy-guide-completion-fragment';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isNoLinkLayout: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsNoLinkLayout_(shouldShowWaa_,' +
            'shouldShowPrivacySandbox_)',
      },

      subheader_: {
        type: String,
        computed: 'computeSubheader_(isNoLinkLayout)',
      },

      shouldShowPrivacySandbox_: {
        type: Boolean,
        value: () => !loadTimeData.getBoolean('isPrivacySandboxRestricted') ||
            loadTimeData.getBoolean('isPrivacySandboxRestrictedNoticeEnabled'),
      },

      shouldShowWaa_: {
        type: Boolean,
        value: false,
      },

      shouldShowV2AdPrivacySubLabel_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private shouldShowPrivacySandbox_: boolean;
  private shouldShowWaa_: boolean;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private shouldShowV2AdPrivacySubLabel_: boolean;
  private privacyGuideBrowserProxy_: PrivacyGuideBrowserProxy =
      PrivacyGuideBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);

    this.addWebUiListener(
        'update-sync-state',
        (event: UpdateSyncStateEvent) => this.updateWaaLink_(event.signedIn));
    ClearBrowsingDataBrowserProxyImpl.getInstance().getSyncState().then(
        (status: UpdateSyncStateEvent) => this.updateWaaLink_(status.signedIn));
    this.privacyGuideBrowserProxy_
        .privacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel()
        .then(state => {
          this.shouldShowV2AdPrivacySubLabel_ = state;
        });
  }

  override focus() {
    this.shadowRoot!.querySelector<HTMLElement>('.headline')!.focus();
  }

  private onViewEnterStart_() {
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE);
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.COMPLETION_REACHED);
  }

  private computeIsNoLinkLayout_() {
    return !this.shouldShowWaa_ && !this.shouldShowPrivacySandbox_;
  }

  private computeSubheader_(): string {
    return this.computeIsNoLinkLayout_() ?
        this.i18n('privacyGuideCompletionCardSubHeaderNoLinks') :
        this.i18n('privacyGuideCompletionCardSubHeader');
  }

  /** Updates the completion card waa link depending on the signin state. */
  private updateWaaLink_(isSignedIn: boolean) {
    this.shouldShowWaa_ = isSignedIn;
  }

  private onBackButtonClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(
        new CustomEvent('back-button-click', {bubbles: true, composed: true}));
  }

  private onLeaveButtonClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
        PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.NextClickCompletion');
    // Send a |close| event to the privacy guide dialog to close itself.
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onPrivacySandboxClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.CompletionPSClick');
    // Create a MouseEvent directly to avoid Polymer failing to synthesise a
    // click event if this function was called in response to a touch event.
    // See crbug.com/1253883 for details.
    // TODO(crbug.com/40162029): Replace this with an ordinary OpenWindowProxy call.
    this.shadowRoot!.querySelector<HTMLAnchorElement>('#privacySandboxLink')!
        .dispatchEvent(new MouseEvent('click'));
  }

  private onWaaClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.SWAA_COMPLETION_LINK);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.CompletionSWAAClick');
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('activityControlsUrlInPrivacyGuide'));
  }

  private computePrivacySandboxRowSubLabel_(): string {
    return this.i18n(
        this.shouldShowV2AdPrivacySubLabel_ ?
            'privacyGuideCompletionCardPrivacySandboxSubLabelAdTopics' :
            'privacyGuideCompletionCardPrivacySandboxSubLabel');
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
