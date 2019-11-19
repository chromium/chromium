// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Object to communicate between the renderer and the browser.
 * @type {!BrowserBridge}
 */
let g_browser = null;

/**
 * This class is the root view object of the page.  It owns all the other
 * views, and manages switching between them.  It is also responsible for
 * initializing the views and the BrowserBridge.
 */
const MainView = (function() {
  'use strict';

  // We inherit from WindowView
  const superClass = WindowView;

  /**
   * Main entry point. Called once the page has loaded.
   *  @constructor
   */
  function MainView() {
    assertFirstConstructorCall(MainView);

    if (hasTouchScreen()) {
      document.body.classList.add('touch');
    }

    // This must be initialized before the tabs, so they can register as
    // observers.
    g_browser = BrowserBridge.getInstance();

    // Create the tab switcher.
    this.initTabs_();
    superClass.call(this, this.tabSwitcher_);

    // Trigger initial layout.
    this.resetGeometry();

    window.onhashchange = this.onUrlHashChange_.bind(this);

    // Select the initial view based on the current URL.
    window.onhashchange();
  }

  cr.addSingletonGetter(MainView);

  MainView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    // This is exposed for testing.
    tabSwitcher: function() {
      return this.tabSwitcher_;
    },

    initTabs_: function() {
      this.tabIdToHash_ = {};
      this.hashToTabId_ = {};

      this.tabSwitcher_ = new TabSwitcherView(this.onTabSwitched_.bind(this));

      // Helper function to add a tab given the class for a view singleton.
      const addTab = function(viewClass) {
        const tabId = viewClass.TAB_ID;
        const tabHash = viewClass.TAB_HASH;
        const tabName = viewClass.TAB_NAME;
        const view = viewClass.getInstance();

        if (!tabId || !view || !tabHash || !tabName) {
          throw Error('Invalid view class for tab');
        }

        if (tabHash.charAt(0) != '#') {
          throw Error('Tab hashes must start with a #');
        }

        this.tabSwitcher_.addTab(tabId, view, tabName, tabHash);
        this.tabIdToHash_[tabId] = tabHash;
        this.hashToTabId_[tabHash] = tabId;
      }.bind(this);

      // Populate the main tabs.  Even tabs that don't contain information for
      // the running OS should be created, so they can load log dumps from other
      // OSes.
      addTab(EventsView);
      addTab(ProxyView);
      addTab(DnsView);
      addTab(SocketsView);
      addTab(DomainSecurityPolicyView);
      addTab(CrosView);

      this.tabSwitcher_.showTabLink(CrosView.TAB_ID, cr.isChromeOS);
    },

    /**
     * This function is called by the tab switcher when the current tab has been
     * changed. It will update the current URL to reflect the new active tab,
     * so the back can be used to return to previous view.
     */
    onTabSwitched_: function(oldTabId, newTabId) {
      // Change the URL to match the new tab.
      const newTabHash = this.tabIdToHash_[newTabId];
      const parsed = parseUrlHash_(window.location.hash);
      if (parsed.tabHash != newTabHash) {
        window.location.hash = newTabHash;
      }
    },

    onUrlHashChange_: function() {
      const parsed = parseUrlHash_(window.location.hash);

      if (!parsed) {
        return;
      }

      // Redirect deleted pages to #events page, which contains instructions
      // about migrating to using net-export and the external netlog_viewer.
      if ([
            '#capture', '#import', '#export', '#timeline', '#alt-svc', '#http2',
            '#quic', '#reporting', '#httpCache', '#modules', '#bandwidth',
            '#prerender'
          ].includes(parsed.tabHash)) {
        parsed.tabHash = EventsView.TAB_HASH;
      }

      // Don't switch to the chromeos view if not on chromeos.
      if (!cr.isChromeOS && parsed.tabHash == '#chromeos') {
        parsed.tabHash = EventsView.TAB_HASH;
      }

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
    },

  };

  /**
   * Takes the current hash in form of "#tab&param1=value1&param2=value2&..."
   * and parses it into a dictionary.
   *
   * Parameters and values are decoded with decodeURIComponent().
   */
  function parseUrlHash_(hash) {
    const parameters = hash.split('&');

    let tabHash = parameters[0];
    if (tabHash == '' || tabHash == '#') {
      tabHash = undefined;
    }

    // Split each string except the first around the '='.
    let paramDict = null;
    for (let i = 1; i < parameters.length; i++) {
      const paramStrings = parameters[i].split('=');
      if (paramStrings.length != 2) {
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

  return MainView;
})();
