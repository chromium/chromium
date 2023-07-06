// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserBridge} from './browser_bridge.js';
// <if expr="chromeos_ash">
import {CrosView} from './chromeos_view.js';
// </if>
import {DnsView} from './dns_view.js';
import {DomainSecurityPolicyView} from './domain_security_policy_view.js';
import {EventsView} from './events_view.js';
import {ProxyView} from './proxy_view.js';
import {SharedDictionaryView} from './shared_dictionary_view.js';
import {SocketsView} from './sockets_view.js';
import {TabSwitcherView} from './tab_switcher_view.js';
import {hasTouchScreen} from './util.js';
import {WindowView} from './view.js';

/** @type {?MainView} */
let instance = null;

/**
 * This class is the root view object of the page.  It owns all the other
 * views, and manages switching between them.  It is also responsible for
 * initializing the views and the BrowserBridge.
 */
export class MainView extends WindowView {
  constructor() {
    if (hasTouchScreen()) {
      document.body.classList.add('touch');
    }

    const tabSwitcher = new TabSwitcherView();
    super(tabSwitcher);
    this.tabSwitcher_ = tabSwitcher;

    // Create the tab switcher.
    this.initTabs_();

    // Trigger initial layout.
    this.resetGeometry();

    window.onhashchange = this.onUrlHashChange_.bind(this);

    // Select the initial view based on the current URL.
    window.onhashchange();
  }

  // This is exposed for testing.
  tabSwitcher() {
    return this.tabSwitcher_;
  }

  initTabs_() {
    this.tabIdToHash_ = {};
    this.hashToTabId_ = {};

    this.tabSwitcher_.setOnTabSwitched(this.onTabSwitched_.bind(this));

    // Helper function to add a tab given the class for a view singleton.
    const addTab = function(viewClass) {
      const tabId = viewClass.TAB_ID;
      const tabHash = viewClass.TAB_HASH;
      const tabName = viewClass.TAB_NAME;
      const view = viewClass.getInstance();

      if (!tabId || !view || !tabHash || !tabName) {
        throw Error('Invalid view class for tab');
      }

      if (tabHash.charAt(0) !== '#') {
        throw Error('Tab hashes must start with a #');
      }

      this.tabSwitcher_.addTab(tabId, view, tabName, tabHash);
      this.tabIdToHash_[tabId] = tabHash;
      this.hashToTabId_[tabHash] = tabId;
    }.bind(this);

    // Populate the main tabs.
    addTab(EventsView);
    addTab(ProxyView);
    addTab(DnsView);
    addTab(SocketsView);
    addTab(DomainSecurityPolicyView);
    addTab(SharedDictionaryView);
    // <if expr="chromeos_ash">
    addTab(CrosView);
    // </if>
  }

  /**
   * This function is called by the tab switcher when the current tab has been
   * changed. It will update the current URL to reflect the new active tab,
   * so the back can be used to return to previous view.
   */
  onTabSwitched_(oldTabId, newTabId) {
    // Change the URL to match the new tab.
    const newTabHash = this.tabIdToHash_[newTabId];
    const parsed = parseUrlHash_(window.location.hash);
    if (parsed.tabHash !== newTabHash) {
      window.location.hash = newTabHash;
    }
  }

  onUrlHashChange_() {
    const parsed = parseUrlHash_(window.location.hash);

    if (!parsed) {
      return;
    }

    // Redirect deleted pages to #events page, which contains instructions
    // about migrating to using net-export and the external netlog_viewer.
    if ([
          '#capture',
          '#import',
          '#export',
          '#timeline',
          '#alt-svc',
          '#http2',
          '#quic',
          '#reporting',
          '#httpCache',
          '#modules',
          '#bandwidth',
          '#prerender',
        ].includes(parsed.tabHash)) {
      parsed.tabHash = EventsView.TAB_HASH;
    }

    // <if expr="not chromeos_ash">
    // Don't switch to the chromeos view if not on chromeos.
    if (parsed.tabHash === '#chromeos') {
      parsed.tabHash = EventsView.TAB_HASH;
    }
    // </if>

    if (!parsed.tabHash) {
      // Default to the events tab.
      parsed.tabHash = EventsView.TAB_HASH;
    }

    const tabId = this.hashToTabId_[parsed.tabHash];

    if (tabId) {
      this.tabSwitcher_.switchToTab(tabId);
      if (parsed.parameters) {
        const view = this.tabSwitcher_.getTabView(tabId);
        view.setParameters(parsed.parameters);
      }
    }
  }

  static getInstance() {
    return instance || (instance = new MainView());
  }
}

/**
 * Takes the current hash in form of "#tab&param1=value1&param2=value2&..."
 * and parses it into a dictionary.
 *
 * Parameters and values are decoded with decodeURIComponent().
 */
function parseUrlHash_(hash) {
  const parameters = hash.split('&');

  let tabHash = parameters[0];
  if (tabHash === '' || tabHash === '#') {
    tabHash = undefined;
  }

  // Split each string except the first around the '='.
  let paramDict = null;
  for (let i = 1; i < parameters.length; i++) {
    const paramStrings = parameters[i].split('=');
    if (paramStrings.length !== 2) {
      continue;
    }
    if (paramDict == null) {
      paramDict = {};
    }
    const key = decodeURIComponent(paramStrings[0]);
    const value = decodeURIComponent(paramStrings[1]);
    paramDict[key] = value;
  }

  return {tabHash: tabHash, parameters: paramDict};
}
