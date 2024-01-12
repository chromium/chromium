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
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TabOrganizationFailureElement} from './tab_organization_failure.js';
import {TabOrganizationInProgressElement} from './tab_organization_in_progress.js';
import {TabOrganizationNotStartedElement} from './tab_organization_not_started.js';
import {getTemplate} from './tab_organization_page.html.js';
import {TabOrganizationResultsElement} from './tab_organization_results.js';
import {Tab, TabOrganization, TabOrganizationError, TabOrganizationSession, TabOrganizationState, UserFeedback} from './tab_search.mojom-webui.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

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
      name_: String,
      tabs_: Array,
      error_: Object,
      availableHeight_: Number,
      isLastOrganization_: Boolean,

      tabOrganizationStateEnum_: {
        type: Object,
        value: TabOrganizationState,
      },

      showFRE_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showTabOrganizationFRE'),
      },
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private state_: TabOrganizationState = TabOrganizationState.kInitializing;
  private name_: string;
  private tabs_: Tab[];
  private error_: TabOrganizationError = TabOrganizationError.kNone;
  private availableHeight_: number = 0;
  private sessionId_: number = -1;
  private organizationId_: number = -1;
  private isLastOrganization_: boolean = false;
  private showFRE_: boolean;
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

    if (this.sessionId_ > -1 && this.organizationId_ > -1) {
      this.apiProxy_.rejectTabOrganization(
          this.sessionId_, this.organizationId_);
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

  setSessionForTesting(session: TabOrganizationSession) {
    this.setSession_(session);
  }

  private setSession_(session: TabOrganizationSession) {
    this.sessionId_ = session.sessionId;
    this.error_ = session.error;
    if (session.state === TabOrganizationState.kSuccess) {
      const organization: TabOrganization = session.organizations[0];
      this.name_ = mojoString16ToString(organization.name);
      this.tabs_ = organization.tabs;
      this.organizationId_ = organization.organizationId;
      this.isLastOrganization_ = session.organizations.length === 1;
    } else {
      this.organizationId_ = -1;
    }
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
    switch (state) {
      case TabOrganizationState.kInitializing:
        break;
      case TabOrganizationState.kNotStarted:
        this.$.notStarted.announceHeader();
        break;
      case TabOrganizationState.kInProgress:
        this.$.inProgress.announceHeader();
        break;
      case TabOrganizationState.kSuccess:
        this.$.results.announceHeader();
        break;
      case TabOrganizationState.kFailure:
        this.$.failure.announceHeader();
        break;
      default:
        assertNotReached('Invalid tab organization state');
    }

    // Ensure the loading state appears for a sufficient amount of time, so as
    // to not appear jumpy if the request completes quickly.
    if (state === TabOrganizationState.kInProgress) {
      this.futureState_ = TabOrganizationState.kInProgress;
      setTimeout(() => this.applyFutureState_(), MIN_LOADING_ANIMATION_MS);
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

  private onRefreshClick_() {
    this.apiProxy_.rejectTabOrganization(this.sessionId_, this.organizationId_);
  }

  private onCreateGroupClick_(event: CustomEvent<{name: string, tabs: Tab[]}>) {
    this.name_ = event.detail.name;
    this.tabs_ = event.detail.tabs;

    this.apiProxy_.acceptTabOrganization(
        this.sessionId_, this.organizationId_, this.name_, this.tabs_);
  }

  private onCheckNow_() {
    this.apiProxy_.restartSession();
  }

  private onTipClick_() {
    this.apiProxy_.startTabGroupTutorial();
  }

  private onRemoveTab_(event: CustomEvent<{tab: Tab}>) {
    this.apiProxy_.removeTabFromOrganization(
        this.sessionId_, this.organizationId_, event.detail.tab);
  }

  private onLearnMoreClick_() {
    this.apiProxy_.openHelpPage();
  }

  private onFeedback_(event: CustomEvent<{value: CrFeedbackOption}>) {
    switch (event.detail.value) {
      case CrFeedbackOption.UNSPECIFIED:
        this.apiProxy_.setUserFeedback(
            this.sessionId_, this.organizationId_,
            UserFeedback.kUserFeedBackUnspecified);
        return;
      case CrFeedbackOption.THUMBS_UP:
        this.apiProxy_.setUserFeedback(
            this.sessionId_, this.organizationId_,
            UserFeedback.kUserFeedBackPositive);
        return;
      case CrFeedbackOption.THUMBS_DOWN:
        this.apiProxy_.setUserFeedback(
            this.sessionId_, this.organizationId_,
            UserFeedback.kUserFeedBackNegative);
        // Show feedback dialog
        this.apiProxy_.triggerFeedback(this.sessionId_);
        return;
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
