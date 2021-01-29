// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {BrowserBridge} from './browser_bridge.js';
import {addNode, addNodeWithText, addTextNode} from './util.js';
import {DivView} from './view.js';

/**
 * This UI allows a user to query and update the browser's list of per-domain
 * security policies. These policies include:
 * - HSTS: HTTPS Strict Transport Security. A way for sites to elect to always
 *   use HTTPS. See http://dev.chromium.org/sts
 * - Expect-CT. A way for sites to elect to always require valid Certificate
 *   Transparency information to be present. See
 *   https://tools.ietf.org/html/draft-ietf-httpbis-expect-ct-01
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
    this.addExpectCTInput_ = $(DomainSecurityPolicyView.ADD_EXPECT_CT_INPUT_ID);
    this.addExpectCTReportUriInput_ =
        $(DomainSecurityPolicyView.ADD_EXPECT_CT_REPORT_URI_INPUT_ID);
    this.addExpectCTEnforceCheck_ =
        $(DomainSecurityPolicyView.ADD_EXPECT_CT_ENFORCE_CHECK_ID);
    this.queryExpectCTInput_ =
        $(DomainSecurityPolicyView.QUERY_EXPECT_CT_INPUT_ID);
    this.queryExpectCTOutputDiv_ =
        $(DomainSecurityPolicyView.QUERY_EXPECT_CT_OUTPUT_DIV_ID);
    this.testExpectCTReportInput_ =
        $(DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_INPUT_ID);
    this.testExpectCTOutputDiv_ =
        $(DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_OUTPUT_DIV_ID);

    let form = $(DomainSecurityPolicyView.DELETE_FORM_ID);
    form.addEventListener('submit', this.onSubmitDelete_.bind(this), false);

    form = $(DomainSecurityPolicyView.ADD_HSTS_FORM_ID);
    form.addEventListener('submit', this.onSubmitHSTSAdd_.bind(this), false);

    form = $(DomainSecurityPolicyView.QUERY_HSTS_FORM_ID);
    form.addEventListener('submit', this.onSubmitHSTSQuery_.bind(this), false);

    form = $(DomainSecurityPolicyView.ADD_EXPECT_CT_FORM_ID);
    form.addEventListener(
        'submit', this.onSubmitExpectCTAdd_.bind(this), false);

    form = $(DomainSecurityPolicyView.QUERY_EXPECT_CT_FORM_ID);
    form.addEventListener(
        'submit', this.onSubmitExpectCTQuery_.bind(this), false);

    form = $(DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_FORM_ID);
    form.addEventListener(
        'submit', this.onSubmitExpectCTTestReport_.bind(this), false);

    this.hstsObservers_ = [];
    this.expectCTObservers_ = [];
  }

  // Test specific functions.
  addHSTSObserverForTest(observer) {
    this.hstsObservers_.push(observer);
  }

  addExpectCTObserverForTest(observer) {
    this.expectCTObservers_.push(observer);
  }

  onSubmitDelete_(event) {
    this.browserBridge_.sendDomainSecurityPolicyDelete(this.deleteInput_.value);
    this.deleteInput_.value = '';
    event.preventDefault();
  }

  onSubmitHSTSAdd_(event) {
    this.browserBridge_.sendHSTSAdd(
        this.addStsInput_.value, this.addStsCheck_.checked);
    this.browserBridge_.sendHSTSQuery(this.addStsInput_.value).then(result => {
      this.onHSTSQueryResult_(result);
    });
    this.queryStsInput_.value = this.addStsInput_.value;
    this.addStsCheck_.checked = false;
    this.addStsInput_.value = '';
    event.preventDefault();
  }

  onSubmitDelete_(event) {
    this.browserBridge_.sendDomainSecurityPolicyDelete(this.deleteInput_.value);
    this.deleteInput_.value = '';
    event.preventDefault();
  }

  onSubmitHSTSQuery_(event) {
    this.browserBridge_.sendHSTSQuery(this.queryStsInput_.value)
        .then(result => {
          this.onHSTSQueryResult_(result);
        });
    event.preventDefault();
  }

  onHSTSQueryResult_(result) {
    this.queryStsOutputDiv_.innerHTML = trustedTypes.emptyHTML;
    if (result.error != undefined) {
      const s = addNode(this.queryStsOutputDiv_, 'span');
      s.textContent = result.error;
      s.style.color = '#e00';
    } else if (result.result == false) {
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
        if (staticHashValue != undefined && staticHashValue != '') {
          staticHashes.push(staticHashValue);
        }

        for (let i = 0; i < keys.length; ++i) {
          const key = keys[i];
          const value = result[key];
          addTextNode(this.queryStsOutputDiv_, ' ' + key + ': ');

          // If there are no static_hashes, do not make it seem like there is a
          // static PKP policy in place.
          if (staticHashes.length == 0 && key.startsWith('static_pkp_')) {
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
                this.queryStsOutputDiv_, 'tt', value == undefined ? '' : value);
          }
          addNode(this.queryStsOutputDiv_, 'br');
        }
      }
    }

    yellowFade(this.queryStsOutputDiv_);
    for (const observer of this.hstsObservers_) {
      observer.onHSTSQueryResult(result);
    }
  }

  onSubmitExpectCTAdd_(event) {
    this.browserBridge_.sendExpectCTAdd(
        this.addExpectCTInput_.value, this.addExpectCTReportUriInput_.value,
        this.addExpectCTEnforceCheck_.checked);
    this.browserBridge_.sendExpectCTQuery(this.addExpectCTInput_.value)
        .then(result => {
          this.onExpectCTQueryResult_(result);
        });
    this.queryExpectCTInput_.value = this.addExpectCTInput_.value;
    this.addExpectCTInput_.value = '';
    this.addExpectCTReportUriInput_.value = '';
    this.addExpectCTEnforceCheck_.checked = false;
    event.preventDefault();
  }

  onSubmitExpectCTQuery_(event) {
    this.browserBridge_.sendExpectCTQuery(this.queryExpectCTInput_.value)
        .then(result => {
          this.onExpectCTQueryResult_(result);
        });

    event.preventDefault();
  }

  onExpectCTQueryResult_(result) {
    this.queryExpectCTOutputDiv_.innerHTML = trustedTypes.emptyHTML;
    if (result.error != undefined) {
      const s = addNode(this.queryExpectCTOutputDiv_, 'span');
      s.textContent = result.error;
      s.style.color = '#e00';
    } else if (result.result == false) {
      const notFound = document.createElement('b');
      notFound.textContent = 'Not found';
      this.queryExpectCTOutputDiv_.appendChild(notFound);
    } else {
      const s = addNode(this.queryExpectCTOutputDiv_, 'span');
      const found = document.createElement('b');
      found.textContent = 'Found:';
      s.appendChild(found);
      s.appendChild(document.createElement('br'));

      const keys = [
        'dynamic_expect_ct_domain',
        'dynamic_expect_ct_observed',
        'dynamic_expect_ct_expiry',
        'dynamic_expect_ct_enforce',
        'dynamic_expect_ct_report_uri',
      ];

      for (const i in keys) {
        const key = keys[i];
        const value = result[key];
        addTextNode(this.queryExpectCTOutputDiv_, ' ' + key + ': ');
        addNodeWithText(
            this.queryExpectCTOutputDiv_, 'tt',
            value == undefined ? '' : value);
        addNode(this.queryExpectCTOutputDiv_, 'br');
      }
    }

    yellowFade(this.queryExpectCTOutputDiv_);
    for (const observer of this.expectCTObservers_) {
      observer.onExpectCTQueryResult(result);
    }
  }

  onSubmitExpectCTTestReport_(event) {
    this.browserBridge_
        .sendExpectCTTestReport(this.testExpectCTReportInput_.value)
        .then(result => {
          if (result === 'invalid') {
            return;
          }
          this.onExpectCTTestReportResult_(result);
        });
    event.preventDefault();
  }

  onExpectCTTestReportResult_(result) {
    addTextNode(
        this.testExpectCTOutputDiv_,
        result === 'success' ? 'Test report succeeded' : 'Test report failed');
    yellowFade(this.testExpectCTOutputDiv_);
    for (const observer of this.expectCTObservers_) {
      observer.onExpectCTTestReportResult(result);
    }
  }
}

function modeToString(m) {
  // These numbers must match those in
  // TransportSecurityState::STSState::UpgradeMode.
  if (m == 0) {
    return 'FORCE_HTTPS';
  } else if (m == 1) {
    return 'DEFAULT';
  } else {
    return 'UNKNOWN';
  }
}

function yellowFade(element) {
  element.style.transitionProperty = 'background-color';
  element.style.transitionDuration = '0';
  element.style.backgroundColor = '#fffccf';
  setTimeout(function() {
    element.style.transitionDuration = '1000ms';
    element.style.backgroundColor = '#fff';
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
// Expect-CT form elements
DomainSecurityPolicyView.ADD_EXPECT_CT_INPUT_ID = 'expect-ct-view-add-input';
DomainSecurityPolicyView.ADD_EXPECT_CT_REPORT_URI_INPUT_ID =
    'expect-ct-view-add-report-uri-input';
DomainSecurityPolicyView.ADD_EXPECT_CT_ENFORCE_CHECK_ID =
    'expect-ct-view-check-enforce-input';
DomainSecurityPolicyView.ADD_EXPECT_CT_FORM_ID = 'expect-ct-view-add-form';
DomainSecurityPolicyView.ADD_EXPECT_CT_SUBMIT_ID = 'expect-ct-view-add-submit';
DomainSecurityPolicyView.QUERY_EXPECT_CT_INPUT_ID =
    'expect-ct-view-query-input';
DomainSecurityPolicyView.QUERY_EXPECT_CT_FORM_ID = 'expect-ct-view-query-form';
DomainSecurityPolicyView.QUERY_EXPECT_CT_SUBMIT_ID =
    'expect-ct-view-query-submit';
DomainSecurityPolicyView.QUERY_EXPECT_CT_OUTPUT_DIV_ID =
    'expect-ct-view-query-output';
DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_INPUT_ID =
    'expect-ct-view-test-report-uri';
DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_FORM_ID =
    'expect-ct-view-test-report-form';
DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_SUBMIT_ID =
    'expect-ct-view-test-report-submit';
DomainSecurityPolicyView.TEST_REPORT_EXPECT_CT_OUTPUT_DIV_ID =
    'expect-ct-view-test-report-output';

addSingletonGetter(DomainSecurityPolicyView);
