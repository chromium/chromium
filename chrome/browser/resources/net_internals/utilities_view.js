// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {BrowserBridge} from './browser_bridge.js';
import {addNode} from './util.js';
import {DivView} from './view.js';

/** @type {?UtilitiesView} */
let instance = null;

/**
 * This view displays a utility to test Proxy Pattern URL Matchers.
 */
export class UtilitiesView extends DivView {
  constructor() {
    super(UtilitiesView.MAIN_BOX_ID);

    this.browserBridge_ = BrowserBridge.getInstance();
    this.matcherInput_ = $(UtilitiesView.MATCHER_INPUT_ID);
    this.urlInput_ = $(UtilitiesView.URL_INPUT_ID);
    this.testOutputDiv_ = $(UtilitiesView.TEST_OUTPUT_ID);
    this.observers_ = [];

    $(UtilitiesView.TEST_FORM_ID)
        .addEventListener(
            'submit', this.onSubmitTestMatcher_.bind(this), false);

    instance = this;
  }

  addObserverForTest(observer) {
    this.observers_.push(observer);
  }

  notifyObservers_(result) {
    for (const observer of this.observers_) {
      observer.onTestMatcherResult(result);
    }
  }

  onSubmitTestMatcher_(event) {
    const matcher = this.matcherInput_.value;
    const url = this.urlInput_.value;
    if (matcher === '' || url === '') {
      return;
    }
    this.testOutputDiv_.innerHTML = trustedTypes.emptyHTML;
    const container = addNode(this.testOutputDiv_, 'div');

    this.browserBridge_.sendTestProxyConfigurationUrlMatcher(matcher, url)
        .then(result => {
          this.urlInput_.value = result.final_url;
          const matched = result.matched;
          const isValid = result.is_valid;
          const isUrlValid = result.is_url_valid;

          if (!isValid) {
            const div = addNode(container, 'div');
            div.textContent = 'Invalid Matcher Format!';
            div.style.fontWeight = 'bold';
            div.style.color = 'orange';
            this.notifyObservers_(result);
            return;
          }

          if (!isUrlValid) {
            const div = addNode(container, 'div');
            div.textContent = 'Invalid Sample URL Format!';
            div.style.fontWeight = 'bold';
            div.style.color = 'orange';
            this.notifyObservers_(result);
            return;
          }

          const div = addNode(container, 'div');
          div.textContent = matched ? 'Matched' : 'Not Matched';
          div.style.fontWeight = 'bold';
          div.style.color = matched ? 'green' : 'red';
          this.notifyObservers_(result);
        })
        .catch(error => {
          const div = addNode(container, 'div');
          div.textContent = `An error occurred: ${error}.`;
          div.style.color = 'red';
          div.style.fontWeight = 'bold';
          this.notifyObservers_({error: error});
        });

    event.preventDefault();
  }

  static getInstance() {
    return instance || (instance = new UtilitiesView());
  }
}

UtilitiesView.TAB_ID = 'tab-handle-utilities';
UtilitiesView.TAB_NAME = 'Utilities';
UtilitiesView.TAB_HASH = '#utilities';

// IDs for special HTML elements in index.html
UtilitiesView.MAIN_BOX_ID = 'utilities-view-tab-content';

UtilitiesView.TEST_FORM_ID = 'utilities-view-test-form';
UtilitiesView.MATCHER_INPUT_ID = 'utilities-view-matcher-input';
UtilitiesView.URL_INPUT_ID = 'utilities-view-url-input';
UtilitiesView.TEST_OUTPUT_ID = 'utilities-view-test-output';
UtilitiesView.TEST_SUBMIT_ID = 'utilities-view-test-submit';
