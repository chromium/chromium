// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {BrowserBridge} from './browser_bridge.js';
import {addNode} from './util.js';
import {DivView} from './view.js';

/**
 * This view displays information on the host resolver:
 *   - Has a button to lookup the host.
 *   - Has a button to clear the host cache.
 */
export class DnsView extends DivView {
  constructor() {
    super(DnsView.MAIN_BOX_ID);

    this.browserBridge_ = BrowserBridge.getInstance();
    this.dnsLookUpInput_ = $(DnsView.DNS_LOOKUP_INPUT_ID);
    this.dnsLookUpOutputDiv_ = $(DnsView.DNS_LOOKUP_OUTPUT_ID);

    $(DnsView.DNS_LOOKUP_FORM_ID)
        .addEventListener(
            'submit', this.onSubmitResolveHost_.bind(this), false);
    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick = () => {
      this.browserBridge_.sendClearHostResolverCache();
    };
  }

  onSubmitResolveHost_(event) {
    const hostname = this.dnsLookUpInput_.value;
    this.dnsLookUpOutputDiv_.innerHTML = trustedTypes.emptyHTML;
    const s = addNode(this.dnsLookUpOutputDiv_, 'span');
    const found = addNode(s, 'b');

    this.browserBridge_.sendResolveHost(this.dnsLookUpInput_.value)
        .then(result => {
          const resolvedAddresses = JSON.stringify(result);
          found.textContent =
              `Resolved IP addresses of "${hostname}": ${resolvedAddresses}.`;
        })
        .catch(error => {
          found.style.color = 'red';
          found.textContent =
              `An error occurred while resolving "${hostname}" (${error}).`;
        });

    this.dnsLookUpInput_.value = '';
    event.preventDefault();
  }
}

DnsView.TAB_ID = 'tab-handle-dns';
DnsView.TAB_NAME = 'DNS';
DnsView.TAB_HASH = '#dns';

// IDs for special HTML elements in dns_view.html
DnsView.MAIN_BOX_ID = 'dns-view-tab-content';

DnsView.DNS_LOOKUP_FORM_ID = 'dns-view-dns-lookup-form';
DnsView.DNS_LOOKUP_INPUT_ID = 'dns-view-dns-lookup-input';
DnsView.DNS_LOOKUP_OUTPUT_ID = 'dns-view-dns-lookup-output';
DnsView.DNS_LOOKUP_SUBMIT_ID = 'dns-view-dns-lookup-submit';
DnsView.CLEAR_CACHE_BUTTON_ID = 'dns-view-clear-cache';

addSingletonGetter(DnsView);
