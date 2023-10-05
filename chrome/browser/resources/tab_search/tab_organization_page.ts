// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import './tab_organization_failure.js';
import './tab_organization_in_progress.js';
import './tab_organization_not_started.js';
import './tab_organization_results.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_page.html.js';
import {Tab} from './tab_search.mojom-webui.js';
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
      name_: String,
      tabs_: Array,

      tabOrganizationStateEnum_: {
        type: Object,
        value: TabOrganizationState,
      },
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private state_: TabOrganizationState = TabOrganizationState.NOT_STARTED;
  private name_: string;
  private tabs_: Tab[];

  static get template() {
    return getTemplate();
  }

  private isState_(state: TabOrganizationState): boolean {
    return this.state_ === state;
  }

  // TODO(emshack): Remove once actual data is available.
  private createTab_(override: Partial<Tab> = {}): Tab {
    return Object.assign(
        {
          active: false,
          alertStates: [],
          index: -1,
          tabId: -1,
          groupId: -1,
          pinned: false,
          title: '',
          url: {url: 'about:blank'},
          isDefaultFavicon: false,
          showIcon: false,
          lastActiveTimeTicks: -1,
          lastActiveElapsedText: '',
        },
        override);
  }

  private onOrganizeTabsClick_() {
    this.apiProxy_.requestTabOrganization();
    // TODO(emshack): Replace placeholders with actual data once available.
    this.tabs_ = [
      this.createTab_({title: 'Tab 1', url: {url: 'https://tab-1.com/'}}),
      this.createTab_({title: 'Tab 2', url: {url: 'https://tab-2.com/'}}),
      this.createTab_({title: 'Tab 3', url: {url: 'https://tab-3.com/'}}),
    ];
    this.name_ = 'Placeholder name';

    // TODO(emshack): Remove once the above triggers an observable state
    // change.
    this.state_ = TabOrganizationState.IN_PROGRESS;
  }

  private onCreateGroupClick_(event: CustomEvent<{name: string, tabs: Tab[]}>) {
    this.name_ = event.detail.name;
    this.tabs_ = event.detail.tabs;

    // TODO(emshack): Implement this & remove the below call
    this.onCycleStateClick_();
  }

  // TODO(emshack): Remove once there's another way to move between states.
  private onCycleStateClick_() {
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
