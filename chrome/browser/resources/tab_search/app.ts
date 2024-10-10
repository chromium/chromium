// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './auto_tab_groups/auto_tab_groups_page.js';
import './declutter/declutter_page.js';
import './tab_organization_selector.js';
import './tab_search_page.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {TabSearchSection} from './tab_search.mojom-webui.js';
import type {TabSearchApiProxy} from './tab_search_api_proxy.js';
import {TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

export class TabSearchAppElement extends CrLitElement {
  static get is() {
    return 'tab-search-app';
  }

  static override get properties() {
    return {
      selectedTabSection_: {type: Object},
      tabNames_: {type: Array},
      tabIcons_: {type: Array},
      tabOrganizationEnabled_: {type: Boolean},
      declutterEnabled_: {type: Boolean},
    };
  }

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  protected selectedTabSection_: TabSearchSection = TabSearchSection.kNone;
  protected tabNames_: string[] = [
    loadTimeData.getString('tabSearchTabName'),
    loadTimeData.getString('tabOrganizationTabName'),
  ];
  protected tabIcons_: string[] =
      ['images/tab_search.svg', 'images/auto_tab_groups.svg'];
  protected tabOrganizationEnabled_: boolean =
      loadTimeData.getBoolean('tabOrganizationEnabled');
  protected declutterEnabled_: boolean =
      loadTimeData.getBoolean('declutterEnabled');

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.apiProxy_.getTabSearchSection().then(
        ({section}) => this.selectedTabSection_ = section);
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(callbackRouter.tabSearchSectionChanged.addListener(
        this.onTabSectionChanged_.bind(this)));
    this.listenerIds_.push(
        callbackRouter.tabOrganizationEnabledChanged.addListener(
            this.onTabOrganizationEnabledChanged_.bind(this)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  private onTabSectionChanged_(section: TabSearchSection) {
    this.selectedTabSection_ = section;
    if (section === TabSearchSection.kOrganize) {
      const organizationSelector =
          this.shadowRoot!.querySelector('tab-organization-selector');
      if (organizationSelector) {
        organizationSelector.maybeLogFeatureShow();
      }
    }
  }

  private onTabOrganizationEnabledChanged_(enabled: boolean) {
    this.tabOrganizationEnabled_ = enabled;
  }

  protected sectionToIndex_(section: TabSearchSection): number {
    switch (section) {
      case TabSearchSection.kNone:
        return -1;
      case TabSearchSection.kSearch:
        return 0;
      case TabSearchSection.kOrganize:
        return 1;
      default:
        assertNotReached();
    }
  }

  private indexToSection(index: number): TabSearchSection {
    switch (index) {
      case -1:
        return TabSearchSection.kNone;
      case 0:
        return TabSearchSection.kSearch;
      case 1:
        return TabSearchSection.kOrganize;
      default:
        assertNotReached();
    }
  }

  protected onSelectedTabIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedTabSection_ = this.indexToSection(e.detail.value);
    if (this.selectedTabSection_ === TabSearchSection.kOrganize &&
        !this.declutterEnabled_) {
      const autoTabGroupsPage =
          this.shadowRoot!.querySelector('auto-tab-groups-page')!;
      autoTabGroupsPage.classList.toggle('changed-state', false);
    }
    if (this.selectedTabSection_ !== TabSearchSection.kNone) {
      this.apiProxy_.setTabSearchSection(this.selectedTabSection_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-app': TabSearchAppElement;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
