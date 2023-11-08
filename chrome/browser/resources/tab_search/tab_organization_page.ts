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

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_page.html.js';
import {Tab, TabOrganization, TabOrganizationError, TabOrganizationSession, TabOrganizationState} from './tab_search.mojom-webui.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

const TRAILING_BODY_SPACING: number = 20;
const TRAILING_FOOTER_SPACING: number = 28;

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
  private sessionId_: number = -1;
  private organizationId_: number = -1;
  private showFRE_: boolean;

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.apiProxy_.getTabOrganizationSession().then(
        ({session}) => this.setSession_(session));
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabOrganizationSessionUpdated.addListener(
            this.setSession_.bind(this)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));

    if (this.sessionId_ > -1 && this.organizationId_ > -1) {
      this.apiProxy_.rejectTabOrganization(
          this.sessionId_, this.organizationId_);
    }
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
    // Wait for a rendering pass so the new state's scroll height is up to date
    // with any new data.
    afterNextRender(this, () => this.setState_(session.state));
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

    let contentsHeight = 0;
    switch (state) {
      case TabOrganizationState.kNotStarted:
        contentsHeight = this.$.notStarted.scrollHeight + TRAILING_BODY_SPACING;
        break;
      case TabOrganizationState.kInProgress:
        contentsHeight = this.$.inProgress.scrollHeight + TRAILING_BODY_SPACING;
        break;
      case TabOrganizationState.kSuccess:
        contentsHeight = this.$.results.scrollHeight + TRAILING_BODY_SPACING;
        break;
      case TabOrganizationState.kFailure:
        contentsHeight = this.$.failure.scrollHeight + TRAILING_BODY_SPACING;
        if (this.showFooter_()) {
          contentsHeight +=
              this.$.footer.scrollHeight + TRAILING_FOOTER_SPACING;
        }
        break;
    }
    this.$.contents.style.height = contentsHeight + 'px';
  }

  private isState_(state: TabOrganizationState): boolean {
    return this.state_ === state;
  }

  private showFooter_(): boolean {
    return this.state_ === TabOrganizationState.kFailure && this.showFRE_;
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

  private onTipClick_() {
    this.apiProxy_.startTabGroupTutorial();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-page': TabOrganizationPageElement;
  }
}

customElements.define(
    TabOrganizationPageElement.is, TabOrganizationPageElement);
