// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DlpClipboardElement} from './dlp_clipboard_tab.js';
import {getTemplate} from './dlp_tabs.html.js';

interface DlpTab {
  // Title used as the tab button's text.
  title: string;

  // Directive used to render the tab's custom element, but is used also as the
  // URL hash when selecting that tab and tab element's ID.
  directive: string;
}

// Set of all DLP tabs. Adding a new entry here will make it automatically
// show in the UI.
const DLP_TABS: DlpTab[] = [{
  title: 'Clipboard',
  directive: DlpClipboardElement.is,
}];

class DlpTabsElement extends CustomElement {
  static get is() {
    return 'dlp-tabs';
  }

  static override get template() {
    return getTemplate();
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
    assert(headersRoot);
    assert(contentRoot);

    for (const tab of DLP_TABS) {
      this.addTab(headersRoot, contentRoot, tab);
    }

    window.onhashchange = () => {
      this.urlHashChanged(window.location.hash);
    };
    this.urlHashChanged(window.location.hash);
  }

  private urlHashChanged(hash: string) {
    hash = (hash || '').split('#').pop() || '';

    const tab =
        DLP_TABS.find(t => t.directive === hash.toLowerCase()) || DLP_TABS[0];
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

  private showTab(tab: DlpTab) {
    const index = DLP_TABS.findIndex(x => x === tab);
    assert(index >= 0);

    this.tabHeaders.forEach(h => (h as Element).classList.remove('active'));
    (this.tabHeaders.item(index) as Element).classList.add('active');

    this.tabContents.forEach(c => this.hideElement(c as HTMLElement));
    this.showElement(this.tabContents.item(index) as HTMLElement);
  }

  private addTab(headersRoot: Element, contentRoot: Element, tab: DlpTab) {
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

customElements.define(DlpTabsElement.is, DlpTabsElement);
