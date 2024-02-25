// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './help_resources_icons.html.js';
import './os_feedback_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './confirmation_page.html.js';
import {FeedbackFlowButtonClickEvent, FeedbackFlowState} from './feedback_flow.js';
import {showScrollingEffects} from './feedback_utils.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';
import {FeedbackAppPostSubmitAction, FeedbackServiceProviderInterface, SendReportStatus} from './os_feedback_ui.mojom-webui.js';

/**
 * @fileoverview
 * 'confirmation-page' is the last step of the feedback tool.
 */

const ConfirmationPageElementBase = I18nMixin(PolymerElement);

export class ConfirmationPageElement extends ConfirmationPageElementBase {
  static get is() {
    return 'confirmation-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sendReportStatus: {type: SendReportStatus, readOnly: false, notify: true},
      isUserLoggedIn: {type: Boolean, readOnly: false, notify: true},
    };
  }

  /** The status of sending the report. */
  sendReportStatus: SendReportStatus|null;
  /**
   * Whether this is the first action taken by the user after sending
   * feedback.
   */
  isFirstAction = true;
  /** Whether the user has logged in (not on oobe or on the login screen). */
  isUserLoggedIn = false;
  private feedbackServiceProvider: FeedbackServiceProviderInterface;

  constructor() {
    super();

    this.feedbackServiceProvider = getFeedbackServiceProvider();
  }

  override ready() {
    super.ready();
    window.addEventListener('beforeunload', () => {
      this.handleEmitMetrics(FeedbackAppPostSubmitAction.kCloseFeedbackApp);
    });
  }

  /**
   * The page shows different information when the device is offline.
   */
  protected isOffline(): boolean {
    return this.sendReportStatus !== null &&
        this.sendReportStatus === SendReportStatus.kDelayed;
  }

  protected getTitle(): string {
    if (this.isOffline()) {
      return this.i18n('confirmationTitleOffline');
    }
    return this.i18n('confirmationTitleOnline');
  }

  protected getMessage(): string {
    if (this.isOffline()) {
      return this.i18n('thankYouNoteOffline');
    }
    return this.i18n('thankYouNoteOnline');
  }

  protected handleBackButtonClicked(e: Event): void {
    e.stopPropagation();

    this.dispatchEvent(new CustomEvent('go-back-click', {
      composed: true,
      bubbles: true,
      detail: {currentState: FeedbackFlowState.CONFIRMATION},
    }));
    this.handleEmitMetrics(FeedbackAppPostSubmitAction.kSendNewReport);
  }

  /** Close the app when user clicks the done button. */
  protected handleDoneButtonClicked(): void {
    this.handleEmitMetrics(FeedbackAppPostSubmitAction.kClickDoneButton);
    window.close();
  }

  /** Open links, including SWA app link and web link. */
  protected handleLinkClicked(e: Event): void {
    e.stopPropagation();
    const currentTarget = e.currentTarget as HTMLElement;
    switch (currentTarget.id) {
      case 'diagnostics':
        this.feedbackServiceProvider.openDiagnosticsApp();
        this.handleEmitMetrics(FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);
        break;
      case 'explore':
        this.feedbackServiceProvider.openExploreApp();
        this.handleEmitMetrics(FeedbackAppPostSubmitAction.kOpenExploreApp);
        break;
      case 'chromebookCommunity':
        // If app locale is not available, default to en.
        OpenWindowProxyImpl.getInstance().openUrl(
            `https://support.google.com/chromebook/?hl=${
                this.i18n('language') || 'en'}#topic=3399709`);
        this.handleEmitMetrics(
            FeedbackAppPostSubmitAction.kOpenChromebookCommunity);
        break;
      default:
        console.warn('unexpected caller id: ', currentTarget.id);
    }
  }

  handleEmitMetrics(action: FeedbackAppPostSubmitAction): void {
    if (this.isFirstAction) {
      this.isFirstAction = false;
      this.feedbackServiceProvider.recordPostSubmitAction(action);
    }
  }

  focusPageTitle(): void {
    const element = this.shadowRoot!.querySelector<HTMLElement>('#pageTitle');
    assert(element);
    element.focus();
  }

  protected onContainerScroll(event: Event): void {
    showScrollingEffects(event, this);
  }
}

declare global {
  interface HTMLElementEventMap {
    'go-back-click': FeedbackFlowButtonClickEvent;
  }

  interface HTMLElementTagNameMap {
    [ConfirmationPageElement.is]: ConfirmationPageElement;
  }
}
customElements.define(ConfirmationPageElement.is, ConfirmationPageElement);
