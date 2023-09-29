// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import './tab_organization_not_started.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_page.html.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

export enum TabOrganizationState {
  NOT_STARTED = 0,
  IN_PROGRESS = 1,
  SUCCESS = 2,
  FAILURE = 3,
}

export class TabOrganizationPageElement extends PolymerElement {
  static get is() {
    return 'tab-organization-page';
  }

  static get properties() {
    return {
      state_: Object,

      tabOrganizationStateEnum_: {
        type: Object,
        value: TabOrganizationState,
      },
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private state_: TabOrganizationState = TabOrganizationState.NOT_STARTED;

  static get template() {
    return getTemplate();
  }

  private isState_(state: TabOrganizationState): boolean {
    return this.state_ === state;
  }

  private onOrganizeTabsClick_() {
    this.apiProxy_.requestTabOrganization();
    // TODO(emshack): Remove once the above triggers an observable state
    // change.
    this.state_ = TabOrganizationState.IN_PROGRESS;
  }

  private onDismissClicked_() {
    this.state_ = TabOrganizationState.NOT_STARTED;
  }

  private onCreateGroupClicked_() {
    // TODO(emshack): Implement this.
  }

  // TODO(emshack): Remove once there's another way to move between states.
  private onCycleStateClicked_() {
    if (this.state_ === TabOrganizationState.IN_PROGRESS) {
      this.state_ = TabOrganizationState.SUCCESS;
    } else if (this.state_ === TabOrganizationState.SUCCESS) {
      this.state_ = TabOrganizationState.FAILURE;
    } else if (this.state_ === TabOrganizationState.FAILURE) {
      this.state_ = TabOrganizationState.NOT_STARTED;
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
