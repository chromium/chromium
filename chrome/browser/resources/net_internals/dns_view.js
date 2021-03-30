// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {BrowserBridge} from './browser_bridge.js';
import {DivView} from './view.js';

/**
 * This view displays information on the host resolver:
 *
 *   - Has a button to clear the host cache.
 */
export class DnsView extends DivView {
  constructor() {
    super(DnsView.MAIN_BOX_ID);

    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick = () => {
      BrowserBridge.getInstance().sendClearHostResolverCache();
    };
  }
}

DnsView.TAB_ID = 'tab-handle-dns';
DnsView.TAB_NAME = 'DNS';
DnsView.TAB_HASH = '#dns';

// IDs for special HTML elements in dns_view.html
DnsView.MAIN_BOX_ID = 'dns-view-tab-content';

DnsView.CLEAR_CACHE_BUTTON_ID = 'dns-view-clear-cache';

addSingletonGetter(DnsView);
