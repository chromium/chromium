// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {BrowserBridge} from './browser_bridge.js';
import {addNode, addNodeWithText, addTextNode} from './util.js';
import {DivView} from './view.js';

/** @type {?DomainSecurityPolicyView} */
let instance = null;

/**
 * This UI allows a user to query and update the browser's list of per-domain
 * security policies. These policies include:
 * - HSTS: HTTPS Strict Transport Security. A way for sites to elect to always
 *   use HTTPS. See https://www.chromium.org/hsts
 * - PKP. A way for sites to pin to specific certification authorities. Only
 * available via manual preloading.
 */

export class DomainSecurityPolicyView extends DivView {
  constructor() {
    // Call superclass's constructor.
    super(DomainSecurityPolicyView.MAIN_BOX_ID);

    this.browserBridge_ = BrowserBridge.getInstance();
    this.deleteInput_ = $(DomainSecurityPolicyView.DELETE_INPUT_ID);
    this.addStsInput_ = $(DomainSecurityPolicyView.ADD_HSTS_INPUT_ID);
    this.addStsCheck_ = $(DomainSecurityPolicyView.ADD_STS_CHECK_ID);
    this.queryStsInput_ = $(DomainSecurityPolicyView.QUERY_HSTS_INPUT_ID);
    this.queryStsOutputDiv_ =
        $(DomainSecurityPolicyView.QUERY_HSTS_OUTPUT_DIV_ID);

    let form = $(DomainSecurityPolicyView.DELETE_FORM_ID);
    form.addEventListener('submit', this.onSubmitDelete_.bind(this), false);

    form = $(DomainSecurityPolicyView.ADD_HSTS_FORM_ID);
    form.addEventListener('submit', this.onSubmitHSTSAdd_.bind(this), false);

    form = $(DomainSecurityPolicyView.QUERY_HSTS_FORM_ID);
    form.addEventListener('submit', this.onSubmitHSTSQuery_.bind(this), false);

    this.hstsObservers_ = [];
  }

  // Test specific functions.
  addHSTSObserverForTest(observer) {
    this.hstsObservers_.push(observer);
  }

  onSubmitDelete_(event) {
    this.browserBridge_.sendDomainSecurityPolicyDelete(
        this.deleteInput_.value.trim());
    this.deleteInput_.value = '';
    event.preventDefault();
  }

  onSubmitHSTSAdd_(event) {
    this.browserBridge_.sendHSTSAdd(
        this.addStsInput_.value.trim(), this.addStsCheck_.checked);
    this.browserBridge_.sendHSTSQuery(this.addStsInput_.value).then(result => {
      this.onHSTSQueryResult_(result);
    });
    this.queryStsInput_.value = this.addStsInput_.value;
    this.addStsCheck_.checked = false;
    this.addStsInput_.value = '';
    event.preventDefault();
  }

  onSubmitHSTSQuery_(event) {
    this.browserBridge_.sendHSTSQuery(this.queryStsInput_.value.trim())
        .then(result => {
          this.onHSTSQueryResult_(result);
        });
    event.preventDefault();
  }

  onHSTSQueryResult_(result) {
    this.queryStsOutputDiv_.innerHTML = trustedTypes.emptyHTML;
    if (result.error !== undefined) {
      const s = addNode(this.queryStsOutputDiv_, 'span');
      s.textContent = result.error;
      s.style.color = '#e00';
    } else if (result.result === false) {
      const notFound = document.createElement('b');
      notFound.textContent = 'Not found';
      this.queryStsOutputDiv_.appendChild(notFound);
    } else {
      const s = addNode(this.queryStsOutputDiv_, 'span');
      const found = document.createElement('b');
      found.textContent = 'Found:';
      s.appendChild(found);
      s.appendChild(document.createElement('br'));

      const keys = [
        'static_sts_domain',
        'static_upgrade_mode',
        'static_sts_include_subdomains',
        'static_sts_observed',
        'static_pkp_domain',
        'static_pkp_include_subdomains',
        'static_pkp_observed',
        'static_spki_hashes',
        'dynamic_sts_domain',
        'dynamic_upgrade_mode',
        'dynamic_sts_include_subdomains',
        'dynamic_sts_observed',
        'dynamic_sts_expiry',
      ];

      const kStaticHashKeys =
          ['public_key_hashes', 'preloaded_spki_hashes', 'static_spki_hashes'];

      const staticHashes = [];
      for (let i = 0; i < kStaticHashKeys.length; ++i) {
        const staticHashValue = result[kStaticHashKeys[i]];
        if (staticHashValue !== undefined && staticHashValue !== '') {
          staticHashes.push(staticHashValue);
        }

        for (let i = 0; i < keys.length; ++i) {
          const key = keys[i];
          const value = result[key];
          addTextNode(this.queryStsOutputDiv_, ' ' + key + ': ');

          // If there are no static_hashes, do not make it seem like there is a
          // static PKP policy in place.
          if (staticHashes.length === 0 && key.startsWith('static_pkp_')) {
            addNode(this.queryStsOutputDiv_, 'br');
            continue;
          }

          if (key === 'static_spki_hashes') {
            addNodeWithText(
                this.queryStsOutputDiv_, 'tt', staticHashes.join(','));
          } else if (key.indexOf('_upgrade_mode') >= 0) {
            addNodeWithText(this.queryStsOutputDiv_, 'tt', modeToString(value));
          } else {
            addNodeWithText(
                this.queryStsOutputDiv_, 'tt',
                value === undefined ? '' : value);
          }
          addNode(this.queryStsOutputDiv_, 'br');
        }
      }
    }

    highlightFade(this.queryStsOutputDiv_);
    for (const observer of this.hstsObservers_) {
      observer.onHSTSQueryResult(result);
    }
  }

  static getInstance() {
    return instance || (instance = new DomainSecurityPolicyView());
  }
}

function modeToString(m) {
  // These numbers must match those in
  // TransportSecurityState::STSState::UpgradeMode.
  if (m === 0) {
    return 'FORCE_HTTPS';
  } else if (m === 1) {
    return 'DEFAULT';
  } else {
    return 'UNKNOWN';
  }
}

function highlightFade(element) {
  element.style.transitionProperty = 'background-color';
  element.style.transitionDuration = '0ms';
  element.style.backgroundColor = 'var(--color-highlight)';
  setTimeout(function() {
    element.style.transitionDuration = '1s';
    element.style.backgroundColor = 'var(--color-background)';
  }, 0);
}

DomainSecurityPolicyView.TAB_ID = 'tab-handle-domain-security-policy';
DomainSecurityPolicyView.TAB_NAME = 'Domain Security Policy';
// This tab was originally limited to HSTS. Even though it now encompasses
// domain security policy more broadly, keep the hash as "#hsts" to preserve
// links/documentation that directs users to chrome://net-internals#hsts.
DomainSecurityPolicyView.TAB_HASH = '#hsts';

// IDs for special HTML elements in domain_security_policy_view.html
DomainSecurityPolicyView.MAIN_BOX_ID =
    'domain-security-policy-view-tab-content';
DomainSecurityPolicyView.DELETE_INPUT_ID =
    'domain-security-policy-view-delete-input';
DomainSecurityPolicyView.DELETE_FORM_ID =
    'domain-security-policy-view-delete-form';
DomainSecurityPolicyView.DELETE_SUBMIT_ID =
    'domain-security-policy-view-delete-submit';
// HSTS form elements
DomainSecurityPolicyView.ADD_HSTS_INPUT_ID = 'hsts-view-add-input';
DomainSecurityPolicyView.ADD_STS_CHECK_ID = 'hsts-view-check-sts-input';
DomainSecurityPolicyView.ADD_PINS_ID = 'hsts-view-add-pins';
DomainSecurityPolicyView.ADD_HSTS_FORM_ID = 'hsts-view-add-form';
DomainSecurityPolicyView.ADD_HSTS_SUBMIT_ID = 'hsts-view-add-submit';
DomainSecurityPolicyView.QUERY_HSTS_INPUT_ID = 'hsts-view-query-input';
DomainSecurityPolicyView.QUERY_HSTS_OUTPUT_DIV_ID = 'hsts-view-query-output';
DomainSecurityPolicyView.QUERY_HSTS_FORM_ID = 'hsts-view-query-form';
DomainSecurityPolicyView.QUERY_HSTS_SUBMIT_ID = 'hsts-view-query-submit';
