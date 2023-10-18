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
        value: loadTimeData.getInteger('tabIndex'),
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
            ['chrome://resources/images/error.svg',
             'chrome://resources/images/error.svg',
    ],
      },

      tabOrganizationEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('tabOrganizationEnabled'),
      },
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private selectedTabIndex_: number;
  private tabNames_: string[];
  private tabIcons_: string[];
  private tabOrganizationEnabled_: boolean;

  static get template() {
    return getTemplate();
  }

  private onSelectedTabChanged_(event: CustomEvent<{value: number}>) {
    this.apiProxy_.setTabIndex(event.detail.value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-app': TabSearchAppElement;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
