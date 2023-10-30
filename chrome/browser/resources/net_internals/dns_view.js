// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {BrowserBridge} from './browser_bridge.js';
import {addNode} from './util.js';
import {DivView} from './view.js';

/** @type {?DnsView} */
let instance = null;

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
    if (hostname === '') {
      return;
    }
    this.dnsLookUpOutputDiv_.innerHTML = trustedTypes.emptyHTML;
    const span = addNode(this.dnsLookUpOutputDiv_, 'span');

    this.browserBridge_.sendResolveHost(this.dnsLookUpInput_.value)
        .then(result => {
          const resolvedAddresses = JSON.stringify(result.resolved_addresses);
          const div = addNode(span, 'div');
          div.textContent =
              `Resolved IP addresses of "${hostname}": ${resolvedAddresses}.`;
          div.style.fontWeight = 'bold';
          if (result.alternative_endpoints.length > 0) {
            result.alternative_endpoints.forEach((endpoint) => {
              const json = JSON.stringify(endpoint);
              const div = addNode(span, 'div');
              div.textContent = `Alternative endpoint: ${json}.`;
              div.style.fontWeight = 'bold';
            });
          } else {
            const div = addNode(span, 'div');
            div.textContent = `No alternative endpoints.`;
            div.style.fontWeight = 'bold';
          }
        })
        .catch(error => {
          const div = addNode(span, 'div');
          div.textContent =
              `An error occurred while resolving "${hostname}" (${error}).`;
          div.style.color = 'red';
          div.style.fontWeight = 'bold';
        });

    this.dnsLookUpInput_.value = '';
    event.preventDefault();
  }

  static getInstance() {
    return instance || (instance = new DnsView());
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
