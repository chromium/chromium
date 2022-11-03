// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {BrowserBridge} from './browser_bridge.js';
import {DivView} from './view.js';

/** @type {?ProxyView} */
let instance = null;

/**
 * This view displays information on the proxy setup:
 *
 *   - Has a button to reload these settings.
 *   - Has a button to clear the cached bad proxies.
 */
export class ProxyView extends DivView {
  constructor() {
    // Call superclass's constructor.
    super(ProxyView.MAIN_BOX_ID);

    // Hook up the UI components.
    $(ProxyView.RELOAD_SETTINGS_BUTTON_ID).onclick = () => {
      BrowserBridge.getInstance().sendReloadProxySettings();
    };
    $(ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID).onclick = () => {
      BrowserBridge.getInstance().sendClearBadProxies();
    };
  }

  static getInstance() {
    return instance || (instance = new ProxyView());
  }
}

ProxyView.TAB_ID = 'tab-handle-proxy';
ProxyView.TAB_NAME = 'Proxy';
ProxyView.TAB_HASH = '#proxy';

// IDs for special HTML elements in proxy_view.html
ProxyView.MAIN_BOX_ID = 'proxy-view-tab-content';
ProxyView.RELOAD_SETTINGS_BUTTON_ID = 'proxy-view-reload-settings';
ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID = 'proxy-view-clear-bad-proxies';
