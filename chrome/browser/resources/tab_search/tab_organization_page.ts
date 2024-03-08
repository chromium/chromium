// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import './strings.m.js';
import './tab_organization_failure.js';
import './tab_organization_in_progress.js';
import './tab_organization_not_started.js';
import './tab_organization_results.js';
import './tab_organization_shared_style.css.js';

import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {TabOrganizationFailureElement} from './tab_organization_failure.js';
import type {TabOrganizationInProgressElement} from './tab_organization_in_progress.js';
import type {TabOrganizationNotStartedElement} from './tab_organization_not_started.js';
import {getTemplate} from './tab_organization_page.html.js';
import type {TabOrganizationResultsElement} from './tab_organization_results.js';
import type {Tab, TabOrganization, TabOrganizationSession} from './tab_search.mojom-webui.js';
import {TabOrganizationState, UserFeedback} from './tab_search.mojom-webui.js';
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

export class TabOrganizationPageElement extends PolymerElement {
  static get is() {
    return 'tab-organization-page';
  }

  static get properties() {
    return {
      state_: Object,
      session_: Object,

      availableHeight_: {
        type: Number,
        value: 0,
      },

      tabOrganizationStateEnum_: {
        type: Object,
        value: TabOrganizationState,
      },

      showFRE_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showTabOrganizationFRE'),
      },

      multiTabOrganization_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('multiTabOrganizationEnabled'),
      },
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private state_: TabOrganizationState = TabOrganizationState.kInitializing;
  private availableHeight_: number;
  private session_: TabOrganizationSession|null;
  private showFRE_: boolean;
  private multiTabOrganization_: boolean;
  private documentVisibilityChangedListener_: () => void;
  private futureState_: TabOrganizationState|null;

  static get template() {
    return getTemplate();
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
    this.classList.toggle('changed-state', changedState);
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
  }

  private applyFutureState_() {
    assert(this.futureState_);
    this.setState_(this.futureState_);
    this.futureState_ = null;
  }

  private isState_(state: TabOrganizationState): boolean {
    return this.state_ === state;
  }

  private onSyncClick_() {
    this.apiProxy_.triggerSync();
  }

  private onSignInClick_() {
    this.apiProxy_.triggerSignIn();
  }

  private onSettingsClick_() {
    this.apiProxy_.openSyncSettings();
  }

  private onOrganizeTabsClick_() {
    this.apiProxy_.requestTabOrganization();
  }

  private onRejectClick_(event: CustomEvent<{organizationId: number}>) {
    this.apiProxy_.rejectTabOrganization(
        this.session_!.sessionId, event.detail.organizationId);
  }

  private onRejectAllGroupsClick_() {
    this.apiProxy_.rejectSession(this.session_!.sessionId);
  }

  private onCreateGroupClick_(
      event: CustomEvent<{organizationId: number, name: string, tabs: Tab[]}>) {
    this.apiProxy_.acceptTabOrganization(
        this.session_!.sessionId, event.detail.organizationId,
        event.detail.name, event.detail.tabs);
  }

  private onCreateAllGroupsClick_(event: CustomEvent<{
    organizations: Array<{organizationId: number, name: string, tabs: Tab[]}>,
  }>) {
    event.detail.organizations.forEach((organization) => {
      this.apiProxy_.acceptTabOrganization(
          this.session_!.sessionId, organization.organizationId,
          organization.name, organization.tabs);
    });
  }

  private onCheckNow_() {
    this.apiProxy_.restartSession();
  }

  private onTipClick_() {
    this.apiProxy_.startTabGroupTutorial();
  }

  private onRemoveTab_(event: CustomEvent<{organizationId: number, tab: Tab}>) {
    this.apiProxy_.removeTabFromOrganization(
        this.session_!.sessionId, event.detail.organizationId,
        event.detail.tab);
  }

  private onLearnMoreClick_() {
    this.apiProxy_.openHelpPage();
  }

  private onFeedback_(event: CustomEvent<{value: CrFeedbackOption}>) {
    if (!this.session_) {
      return;
    }
    const organizations: TabOrganization[] = this.multiTabOrganization_ ?
        this.session_.organizations :
        this.session_.organizations.slice(0, 1);
    organizations.forEach((organization) => {
      switch (event.detail.value) {
        case CrFeedbackOption.UNSPECIFIED:
          this.apiProxy_.setUserFeedback(
              this.session_!.sessionId, organization.organizationId,
              UserFeedback.kUserFeedBackUnspecified);
          break;
        case CrFeedbackOption.THUMBS_UP:
          this.apiProxy_.setUserFeedback(
              this.session_!.sessionId, organization.organizationId,
              UserFeedback.kUserFeedBackPositive);
          break;
        case CrFeedbackOption.THUMBS_DOWN:
          this.apiProxy_.setUserFeedback(
              this.session_!.sessionId, organization.organizationId,
              UserFeedback.kUserFeedBackNegative);
          break;
      }
    });
    if (event.detail.value === CrFeedbackOption.THUMBS_DOWN) {
      // Show feedback dialog
      this.apiProxy_.triggerFeedback(this.session_.sessionId);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-page': TabOrganizationPageElement;
  }
}

customElements.define(
    TabOrganizationPageElement.is, TabOrganizationPageElement);
