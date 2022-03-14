// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-completion-fragment' is the fragment in a privacy guide
 * card that contains the completion screen and its description.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import './privacy_guide_completion_link_row.js';
import './privacy_guide_fragment_shared_css.js';

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {UpdateSyncStateEvent} from '../../clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
import {loadTimeData} from '../../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../../metrics_browser_proxy.js';
import {OpenWindowProxyImpl} from '../../open_window_proxy.js';
import {SyncBrowserProxyImpl, SyncStatus} from '../../people_page/sync_browser_proxy.js';
import {Router} from '../../router.js';

import {getTemplate} from './privacy_guide_completion_fragment.html.js';

const PrivacyGuideCompletionFragmentElementBase =
    WebUIListenerMixin(PolymerElement);

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
      shouldShowWaa_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private shouldShowWaa_: boolean;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    SyncBrowserProxyImpl.getInstance().getSyncStatus().then(
        (status: SyncStatus) => this.updateWaaLink_(status.signedIn!));
    this.addWebUIListener(
        'update-sync-state',
        (event: UpdateSyncStateEvent) => this.updateWaaLink_(event.signedIn));
  }

  /**
   * Updates the completion card waa link depending on the signin state.
   */
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
    Router.getInstance().navigateToPreviousRoute();
  }

  private onPrivacySandboxClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.CompletionPSClick');
    // Create a MouseEvent directly to avoid Polymer failing to synthesise a
    // click event if this function was called in response to a touch event.
    // See crbug.com/1253883 for details.
    // TODO(crbug/1159942): Replace this with an ordinary OpenWindowProxy call.
    this.shadowRoot!.querySelector<HTMLAnchorElement>('#privacySandboxLink')!
        .dispatchEvent(new MouseEvent('click'));
  }

  private onWaaClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.SWAA_COMPLETION_LINK);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.CompletionSWAAClick');
    OpenWindowProxyImpl.getInstance().openURL(
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
