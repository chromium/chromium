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
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_page.html.js';
import {Tab, TabOrganization, TabOrganizationError, TabOrganizationSession, TabOrganizationState, UserFeedback} from './tab_search.mojom-webui.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

const BODY_VERTICAL_MARGIN: number = 32;
const HEIGHT_ANIMATION_LENGTH: number = 250;

export interface TabOrganizationPageElement {
  $: {
    contents: HTMLElement,
    notStarted: HTMLElement,
    inProgress: HTMLElement,
    results: HTMLElement,
    failure: HTMLElement,
    footer: HTMLElement,
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

      tabOrganizationStateEnum_: {
        type: Object,
        value: TabOrganizationState,
      },

      showFRE_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showTabOrganizationFRE'),
      },
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private state_: TabOrganizationState = TabOrganizationState.kNotStarted;
  private name_: string;
  private tabs_: Tab[];
  private error_: TabOrganizationError = TabOrganizationError.kNone;
  private availableHeight_: number = 0;
  private sessionId_: number = -1;
  private organizationId_: number = -1;
  private showFRE_: boolean;
  private documentVisibilityChangedListener_: () => void;

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

  updateContentsHeightAfterNextRender() {
    afterNextRender(this, () => this.updateContentsHeight_());
  }

  private updateContentsHeight_() {
    let contentsHeight = 0;
    switch (this.state_) {
      case TabOrganizationState.kNotStarted:
        // Subtract padding out here and below as this is variable during
        // animation and should not affect contents height.
        contentsHeight = this.$.notStarted.scrollHeight -
            this.getPaddingTopValue_(this.$.notStarted) + BODY_VERTICAL_MARGIN;
        break;
      case TabOrganizationState.kInProgress:
        contentsHeight = this.$.inProgress.scrollHeight -
            this.getPaddingTopValue_(this.$.inProgress) + BODY_VERTICAL_MARGIN;
        break;
      case TabOrganizationState.kSuccess:
        contentsHeight = this.$.results.scrollHeight -
            this.getPaddingTopValue_(this.$.results) + BODY_VERTICAL_MARGIN;
        break;
      case TabOrganizationState.kFailure:
        contentsHeight = this.$.failure.scrollHeight -
            this.getPaddingTopValue_(this.$.failure) + BODY_VERTICAL_MARGIN;
        if (this.showFRE_) {
          // If the failure footer is shown, exclude bottom margin as the
          // footer should extend to the bottom of the bubble.
          contentsHeight -= BODY_VERTICAL_MARGIN / 2;
        }
        break;
    }
    this.$.contents.style.height = contentsHeight + 'px';
  }

  private onVisible_() {
    // When the UI goes from not shown to shown, bypass height transition.
    this.$.contents.classList.toggle('no-transition', true);
    this.updateAvailableHeight_();
    // TODO(emshack): We should find a way to avoid using a timeout here.
    setTimeout(
        () => this.$.contents.classList.toggle('no-transition', false),
        HEIGHT_ANIMATION_LENGTH);
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
      this.updateContentsHeight_();
    });
  }

  private getPaddingTopValue_(element: HTMLElement): number {
    const pxValue = getComputedStyle(element).getPropertyValue('padding-top');
    return Number.parseInt(pxValue.trim().slice(0, -2), 10);
  }

  private setSession_(session: TabOrganizationSession) {
    this.sessionId_ = session.sessionId;
    this.error_ = session.error;
    if (session.state === TabOrganizationState.kSuccess) {
      const organization: TabOrganization = session.organizations[0];
      this.name_ = mojoString16ToString(organization.name);
      this.tabs_ = organization.tabs;
      this.organizationId_ = organization.organizationId;
    } else {
      this.organizationId_ = -1;
    }
    this.setState_(session.state);
  }

  private setState_(state: TabOrganizationState) {
    this.classList.toggle('changed-state', this.state_ !== state);
    this.classList.toggle(
        'from-not-started', this.state_ === TabOrganizationState.kNotStarted);
    this.classList.toggle(
        'from-in-progress', this.state_ === TabOrganizationState.kInProgress);
    this.classList.toggle(
        'from-success', this.state_ === TabOrganizationState.kSuccess);
    this.classList.toggle(
        'from-failure', this.state_ === TabOrganizationState.kFailure);
    this.state_ = state;
    // Wait for a rendering pass so the new state's scroll height is up to date
    // with any new data.
    this.updateContentsHeightAfterNextRender();
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

  private onCreateGroupClick_(event: CustomEvent<{name: string, tabs: Tab[]}>) {
    this.name_ = event.detail.name;
    this.tabs_ = event.detail.tabs;

    this.apiProxy_.acceptTabOrganization(
        this.sessionId_, this.organizationId_, this.name_, this.tabs_);
  }

  private onCheckNow_() {
    this.apiProxy_.resetSession();
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
