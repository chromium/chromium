// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab_organization_page.js';
import './tab_search_page.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

export class TabSearchAppElement extends PolymerElement {
  static get is() {
    return 'tab-search-app';
  }

  static get properties() {
    return {
      selectedTabIndex_: {
        type: Number,
        value: () => loadTimeData.getInteger('tabIndex'),
      },

      tabNames_: {
        type: Array,
        value: () =>
            [loadTimeData.getString('tabSearchTabName'),
             loadTimeData.getString('tabOrganizationTabName')],
      },

      tabIcons_: {
        type: Array,
        value: () =>
            ['images/tab_search.svg',
             'images/auto_tab_groups.svg',
    ],
      },

      tabOrganizationEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('tabOrganizationEnabled'),
      },
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private selectedTabIndex_: number;
  private tabNames_: string[];
  private tabIcons_: string[];
  private tabOrganizationEnabled_: boolean;

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(callbackRouter.tabSearchTabIndexChanged.addListener(
        this.onTabIndexChanged_.bind(this)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  private onTabIndexChanged_(index: number) {
    this.selectedTabIndex_ = index;
  }

  private onSelectedTabChanged_(event: CustomEvent<{value: number}>) {
    if (event.detail.value === 1) {
      const tabOrganizationPage =
          this.shadowRoot!.querySelector('tab-organization-page')!;
      tabOrganizationPage.classList.toggle('changed-state', false);
    }
    this.apiProxy_.setTabIndex(event.detail.value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-app': TabSearchAppElement;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
