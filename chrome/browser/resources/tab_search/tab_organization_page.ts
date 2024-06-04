// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './tab_organization_failure.js';
import './tab_organization_in_progress.js';
import './tab_organization_not_started.js';
import './tab_organization_results.js';

import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TabOrganizationFailureElement} from './tab_organization_failure.js';
import type {TabOrganizationInProgressElement} from './tab_organization_in_progress.js';
import type {TabOrganizationNotStartedElement} from './tab_organization_not_started.js';
import {getCss} from './tab_organization_page.css.js';
import {getHtml} from './tab_organization_page.html.js';
import type {TabOrganizationResultsElement} from './tab_organization_results.js';
import type {Tab, TabOrganization, TabOrganizationSession} from './tab_search.mojom-webui.js';
import {TabOrganizationError, TabOrganizationState, UserFeedback} from './tab_search.mojom-webui.js';
import type {TabSearchApiProxy} from './tab_search_api_proxy.js';
import {TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

const MIN_LOADING_ANIMATION_MS: number = 500;

export interface TabOrganizationPageElement {
  $: {
    notStarted: TabOrganizationNotStartedElement,
    inProgress: TabOrganizationInProgressElement,
    results: TabOrganizationResultsElement,
    failure: TabOrganizationFailureElement,
  };
}

export class TabOrganizationPageElement extends CrLitElement {
  static get is() {
    return 'tab-organization-page';
  }

  static override get properties() {
    return {
      state_: {type: Number},
      session_: {type: Object},
      availableHeight_: {type: Number},
      showFRE_: {type: Boolean},
      multiTabOrganization_: {type: Boolean},
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private state_: TabOrganizationState = TabOrganizationState.kInitializing;
  protected availableHeight_: number = 0;
  protected session_: TabOrganizationSession|null;
  protected showFRE_: boolean =
      loadTimeData.getBoolean('showTabOrganizationFRE');
  protected multiTabOrganization_: boolean =
      loadTimeData.getBoolean('multiTabOrganizationEnabled');
  private documentVisibilityChangedListener_: () => void;
  private futureState_: TabOrganizationState|null;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  constructor() {
    super();
    this.documentVisibilityChangedListener_ = () => {
      if (document.visibilityState === 'visible') {
        this.onVisible_();
      }
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.apiProxy_.getTabOrganizationSession().then(
        ({session}) => this.setSession_(session));
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabOrganizationSessionUpdated.addListener(
            this.setSession_.bind(this)));
    this.listenerIds_.push(
        callbackRouter.showFREChanged.addListener(this.setShowFre_.bind(this)));
    if (document.visibilityState === 'visible') {
      this.onVisible_();
    }
    document.addEventListener(
        'visibilitychange', this.documentVisibilityChangedListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
    document.removeEventListener(
        'visibilitychange', this.documentVisibilityChangedListener_);

    if (!this.session_) {
      return;
    }
    if (this.multiTabOrganization_) {
      this.session_.organizations.forEach((organization: TabOrganization) => {
        this.apiProxy_.rejectTabOrganization(
            this.session_!.sessionId, organization.organizationId);
      });
    } else {
      this.apiProxy_.rejectTabOrganization(
          this.session_.sessionId,
          this.session_.organizations[0].organizationId);
    }
  }

  private onVisible_() {
    this.updateAvailableHeight_();
    // When the UI goes from not shown to shown, bypass any state change
    // animations.
    this.classList.toggle('changed-state', false);
  }

  // TODO(emshack): Consider moving the available height calculation into
  // app.ts and reusing across both tab search and tab organization.
  private updateAvailableHeight_() {
    this.apiProxy_.getProfileData().then(({profileData}) => {
      // In rare cases there is no browser window. I suspect this happens during
      // browser shutdown.
      if (!profileData.windows) {
        return;
      }
      // TODO(crbug.com/c/1349350): Determine why no active window is reported
      // in some cases on ChromeOS and Linux.
      const activeWindow = profileData.windows.find((t) => t.active);
      this.availableHeight_ =
          activeWindow ? activeWindow!.height : profileData.windows[0]!.height;
    });
  }

  private setShowFre_(show: boolean) {
    this.showFRE_ = show;
  }

  setSessionForTesting(session: TabOrganizationSession) {
    this.setSession_(session);
  }

  private setSession_(session: TabOrganizationSession) {
    this.session_ = session;
    this.maybeSetState_(session.state);
  }

  private maybeSetState_(state: TabOrganizationState) {
    if (this.futureState_) {
      this.futureState_ = state;
      return;
    }
    this.setState_(state);
  }

  private setState_(state: TabOrganizationState) {
    const changedState = this.state_ !== state;
    const wasInitializing = this.state_ === TabOrganizationState.kInitializing;
    this.classList.toggle('changed-state', changedState && !wasInitializing);
    this.classList.toggle(
        'from-not-started', this.state_ === TabOrganizationState.kNotStarted);
    this.classList.toggle(
        'from-in-progress', this.state_ === TabOrganizationState.kInProgress);
    this.classList.toggle(
        'from-success', this.state_ === TabOrganizationState.kSuccess);
    this.classList.toggle(
        'from-failure', this.state_ === TabOrganizationState.kFailure);
    this.state_ = state;
    if (!changedState) {
      return;
    }
    if (state === TabOrganizationState.kInProgress) {
      // Ensure the loading state appears for a sufficient amount of time, so as
      // to not appear jumpy if the request completes quickly.
      this.futureState_ = TabOrganizationState.kInProgress;
      setTimeout(() => this.applyFutureState_(), MIN_LOADING_ANIMATION_MS);
    } else if (state === TabOrganizationState.kSuccess) {
      // Wait until the new state is visible after the transition to focus on
      // the new UI.
      this.$.results.addEventListener('animationend', () => {
        this.$.results.focusInput();
      }, {once: true});
    }
    if (wasInitializing) {
      this.apiProxy_.notifyOrganizationUiReadyToShow();
    }
  }

  private applyFutureState_() {
    assert(this.futureState_);
    this.setState_(this.futureState_);
    this.futureState_ = null;
  }

  protected isState_(state: TabOrganizationState): boolean {
    return this.state_ === state;
  }

  protected onSyncClick_() {
    this.apiProxy_.triggerSync();
  }

  protected onSignInClick_() {
    this.apiProxy_.triggerSignIn();
  }

  protected onSettingsClick_() {
    this.apiProxy_.openSyncSettings();
  }

  protected onOrganizeTabsClick_() {
    this.apiProxy_.requestTabOrganization();
  }

  protected onRejectClick_(event: CustomEvent<{organizationId: number}>) {
    this.apiProxy_.rejectTabOrganization(
        this.session_!.sessionId, event.detail.organizationId);
  }

  protected onRejectAllGroupsClick_() {
    this.apiProxy_.rejectSession(this.session_!.sessionId);
  }

  protected onCreateGroupClick_(
      event: CustomEvent<{organizationId: number, name: string, tabs: Tab[]}>) {
    this.apiProxy_.acceptTabOrganization(
        this.session_!.sessionId, event.detail.organizationId,
        event.detail.name, event.detail.tabs);
  }

  protected onCreateAllGroupsClick_(event: CustomEvent<{
    organizations: Array<{organizationId: number, name: string, tabs: Tab[]}>,
  }>) {
    event.detail.organizations.forEach((organization) => {
      this.apiProxy_.acceptTabOrganization(
          this.session_!.sessionId, organization.organizationId,
          organization.name, organization.tabs);
    });
  }

  protected onCheckNow_() {
    this.apiProxy_.restartSession();
  }

  protected onTipClick_() {
    this.apiProxy_.startTabGroupTutorial();
  }

  protected onRemoveTab_(event:
                             CustomEvent<{organizationId: number, tab: Tab}>) {
    this.apiProxy_.removeTabFromOrganization(
        this.session_!.sessionId, event.detail.organizationId,
        event.detail.tab);
  }

  protected onLearnMoreClick_() {
    this.apiProxy_.openHelpPage();
  }

  protected onFeedback_(event: CustomEvent<{value: CrFeedbackOption}>) {
    if (!this.session_) {
      return;
    }
    // Multi organization feedback is per-session, single organization feedback
    // is per-organization.
    let organizationId = -1;
    if (!this.multiTabOrganization_) {
      organizationId = this.session_.organizations[0]!.organizationId;
    }
    switch (event.detail.value) {
      case CrFeedbackOption.UNSPECIFIED:
        this.apiProxy_.setUserFeedback(
            this.session_!.sessionId, organizationId,
            UserFeedback.kUserFeedBackUnspecified);
        break;
      case CrFeedbackOption.THUMBS_UP:
        this.apiProxy_.setUserFeedback(
            this.session_!.sessionId, organizationId,
            UserFeedback.kUserFeedBackPositive);
        break;
      case CrFeedbackOption.THUMBS_DOWN:
        this.apiProxy_.setUserFeedback(
            this.session_!.sessionId, organizationId,
            UserFeedback.kUserFeedBackNegative);
        break;
    }
    if (event.detail.value === CrFeedbackOption.THUMBS_DOWN) {
      // Show feedback dialog
      this.apiProxy_.triggerFeedback(this.session_.sessionId);
    }
  }

  protected getSessionError_(): TabOrganizationError {
    return this.session_?.error || TabOrganizationError.kNone;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-page': TabOrganizationPageElement;
  }
}

customElements.define(
    TabOrganizationPageElement.is, TabOrganizationPageElement);
