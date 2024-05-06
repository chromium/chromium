// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './connectors_tabs.html.js';
import {DeviceTrustConnectorElement} from './device_trust_connector.js';
import {ManagedClientCertificateElement} from './managed_client_certificate.js';

interface ConnectorTab {
  // Title used as the tab button's text.
  title: string;

  // Directive used to render the tab's custom element, but is used also as the
  // URL hash when selecting that tab and tab element's ID.
  directive: string;

  // Whether the connector is enabled or not. Only show the tab if the connector
  // is enabled.
  isEnabled: boolean;
}

// Set of all connector tabs. Adding a new entry here will make it automatically
// show in the UI.
const connectorTabs: ConnectorTab[] = [
  {
    title: 'Device Trust',
    directive: DeviceTrustConnectorElement.is,
    isEnabled: true,
  },
  {
    title: 'Managed Client Certificate',
    directive: ManagedClientCertificateElement.is,
    isEnabled: true,
  },
];

class ConnectorsTabsElement extends CustomElement {
  static get is() {
    return 'connectors-tabs';
  }

  static override get template() {
    return getTemplate();
  }

  private get tabHeaders() {
    return this.$all('.tabs > button');
  }

  private get tabContents() {
    return this.$all<HTMLElement>('.content > div');
  }

  private get noConnectorsMessage(): HTMLElement {
    return this.getRequiredElement('#no-connectors-message');
  }

  private readonly enabledTabs: ConnectorTab[] =
      connectorTabs.filter(x => x.isEnabled);

  constructor() {
    super();

    // Exit early if no connectors are enabled.
    if (!this.enabledTabs.length) {
      this.showElement(this.noConnectorsMessage);
      return;
    }

    // Add tabs dynamically.
    const headersRoot = this.$('.tabs');
    const contentRoot = this.$('.content');
    if (!headersRoot || !contentRoot) {
      console.error('Could not find headersRoot or contentRoot.');
      return;
    }

    for (const tab of this.enabledTabs) {
      if (tab.isEnabled) {
        this.addTab(headersRoot, contentRoot, tab);
      }
    }

    window.onhashchange = () => {
      this.urlHashChanged(window.location.hash);
    };
    this.urlHashChanged(window.location.hash);
  }

  private urlHashChanged(hash: string) {
    hash = (hash || '').split('#').pop() || '';

    const tab =
        this.enabledTabs.find(t => t.directive === hash.toLowerCase()) ||
        this.enabledTabs[0];
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
    const index = this.enabledTabs.findIndex(x => x === tab);
    if (index < 0) {
      console.error(`Tab ${
          tab.directive} was not found in the array of enabled connectors.`);
      return;
    }

    this.tabHeaders.forEach(h => h.classList.remove('active'));
    this.tabHeaders.item(index).classList.add('active');

    this.tabContents.forEach(c => this.hideElement(c));
    this.showElement(this.tabContents.item(index));
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
    contentElement.appendChild(document.createElement(tab.directive));
    contentRoot.appendChild(contentElement);
  }

  private showElement(element: Element) {
    element?.classList.remove('hidden');
  }

  private hideElement(element: HTMLElement) {
    element?.classList.add('hidden');
  }
}

customElements.define(ConnectorsTabsElement.is, ConnectorsTabsElement);
