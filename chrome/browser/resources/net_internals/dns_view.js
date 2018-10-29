// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the host resolver:
 *
 *   - Has a button to clear the host cache.
 */
var DnsView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   *  @constructor
   */
  function DnsView() {
    assertFirstConstructorCall(DnsView);

    // Call superclass's constructor.
    superClass.call(this, DnsView.MAIN_BOX_ID);

    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick =
        g_browser.sendClearHostResolverCache.bind(g_browser);
  }

  DnsView.TAB_ID = 'tab-handle-dns';
  DnsView.TAB_NAME = 'DNS';
  DnsView.TAB_HASH = '#dns';

  // IDs for special HTML elements in dns_view.html
  DnsView.MAIN_BOX_ID = 'dns-view-tab-content';

  DnsView.CLEAR_CACHE_BUTTON_ID = 'dns-view-clear-cache';

  cr.addSingletonGetter(DnsView);

  DnsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,
  };

  return DnsView;
})();
