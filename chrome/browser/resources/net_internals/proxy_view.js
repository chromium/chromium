// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {BrowserBridge} from './browser_bridge.js';
import {DivView} from './view.js';

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
}

ProxyView.TAB_ID = 'tab-handle-proxy';
ProxyView.TAB_NAME = 'Proxy';
ProxyView.TAB_HASH = '#proxy';

// IDs for special HTML elements in proxy_view.html
ProxyView.MAIN_BOX_ID = 'proxy-view-tab-content';
ProxyView.RELOAD_SETTINGS_BUTTON_ID = 'proxy-view-reload-settings';
ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID = 'proxy-view-clear-bad-proxies';

addSingletonGetter(ProxyView);
