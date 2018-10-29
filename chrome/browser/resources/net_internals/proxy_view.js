// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the proxy setup:
 *
 *   - Has a button to reload these settings.
 *   - Has a button to clear the cached bad proxies.
 */
var ProxyView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function ProxyView() {
    assertFirstConstructorCall(ProxyView);

    // Call superclass's constructor.
    superClass.call(this, ProxyView.MAIN_BOX_ID);

    // Hook up the UI components.
    $(ProxyView.RELOAD_SETTINGS_BUTTON_ID).onclick =
        g_browser.sendReloadProxySettings.bind(g_browser);
    $(ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID).onclick =
        g_browser.sendClearBadProxies.bind(g_browser);
  }

  ProxyView.TAB_ID = 'tab-handle-proxy';
  ProxyView.TAB_NAME = 'Proxy';
  ProxyView.TAB_HASH = '#proxy';

  // IDs for special HTML elements in proxy_view.html
  ProxyView.MAIN_BOX_ID = 'proxy-view-tab-content';
  ProxyView.RELOAD_SETTINGS_BUTTON_ID = 'proxy-view-reload-settings';
  ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID = 'proxy-view-clear-bad-proxies';

  cr.addSingletonGetter(ProxyView);

  ProxyView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,
  };

  return ProxyView;
})();
