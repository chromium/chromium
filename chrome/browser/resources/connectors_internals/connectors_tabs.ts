// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {ZeroTrustConnectorElement} from './zero_trust_connector.js';

interface ConnectorTab {
  // Title used as the tab button's text.
  title: string;

  // Directive used to render the tab's custom element, but is used also as the
  // URL hash when selecting that tab and tab element's ID.
  directive: string;
}

// Set of all connector tabs. Adding a new entry here will make it automatically
// show in the UI.
const connectorTabs: ConnectorTab[] =
    [{title: 'Zero Trust', directive: ZeroTrustConnectorElement.is}];

export class ConnectorsTabsElement extends CustomElement {
  static get is() {
    return 'connectors-tabs';
  }

  static get template() {
    return `{__html_template__}`;
  }

  private get tabHeaders(): NodeList {
    return this.$all('.tabs > button');
  }

  private get tabContents(): NodeList {
    return this.$all('.content > div');
  }

  constructor() {
    super();

    // Add tabs dynamically.
    const headersRoot = this.$('.tabs');
    const contentRoot = this.$('.content');
    if (!headersRoot || !contentRoot) {
      console.error('Could not find headersRoot or contentRoot.');
      return;
    }

    for (const tab of connectorTabs) {
      this.addTab(headersRoot, contentRoot, tab);
    }

    window.onhashchange = () => {
      this.urlHashChanged(window.location.hash);
    };
    this.urlHashChanged(window.location.hash);
  }

  private urlHashChanged(hash: string) {
    hash = (hash || '').split('#').pop() || '';

    const tab = connectorTabs.find(t => t.directive === hash.toLowerCase()) ||
        connectorTabs[0];
    if (tab) {
      this.showTab(tab);
    } else {
      console.error(`Could not find tab for hash '${
          hash}', and no default tab was available.'`);
    }
  }

  private onTabSelected(tabId: string) {
    // Update URL hash, which will trigger tab selection.
    window.location.hash = tabId;
  }

  private showTab(tab: ConnectorTab) {
    const index = connectorTabs.findIndex(x => x === tab);
    if (index < 0) {
      console.error(
          `Tab ${tab.directive} was not found in connectorTabs array.`);
      return;
    }

    this.tabHeaders.forEach(h => (h as Element).classList.remove('active'));
    (this.tabHeaders.item(index) as Element).classList.add('active');

    this.tabContents.forEach(c => (c as HTMLElement).style.display = 'none');
    (this.tabContents.item(index) as HTMLElement).style.display = 'block';
  }

  private addTab(
      headersRoot: Element, contentRoot: Element, tab: ConnectorTab) {
    const headerElement = document.createElement('button');
    headerElement.innerText = tab.title;
    headerElement.addEventListener(
        'click', () => this.onTabSelected(tab.directive));
    headersRoot.appendChild(headerElement);

    const contentElement = document.createElement('div');
    contentElement.classList.add('tabcontent');
    contentElement.id = tab.directive;
    contentElement.innerHTML = `<${tab.directive}></${tab.directive}>`;
    contentRoot.appendChild(contentElement);
  }
}

customElements.define(ConnectorsTabsElement.is, ConnectorsTabsElement);