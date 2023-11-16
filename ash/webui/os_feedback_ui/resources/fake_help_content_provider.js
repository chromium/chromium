// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';

import {HelpContentProviderInterface, SearchRequest, SearchResponse} from './os_feedback_ui.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the HelpContentProvider mojo interface.
 */

/** @implements {HelpContentProviderInterface} */
export class FakeHelpContentProvider {
  constructor() {
    this.methods_ = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods_.register('getHelpContents');

    /**
     * Record the last query passed to getHelpContents to help verify the method
     * has been called.
     * @private {string}
     */
    this.lastQuery_ = '';

    /**
     * Keep track of how many times the getHelpContents has been called.
     * @private {!number}
     */
    this.getHelpContentsCallCount_ = 0;
  }

  /** @return {string} */
  get lastQuery() {
    return this.lastQuery_;
  }

  /**
   * @param {!SearchRequest} request
   * @return {!Promise<{response: !SearchResponse}>}
   */
  getHelpContents(request) {
    ++this.getHelpContentsCallCount_;
    this.lastQuery_ = mojoString16ToString(request.query);
    return this.methods_.resolveMethod('getHelpContents');
  }

  /**
   * @return {number}
   */
  getHelpContentsCallCount() {
    return this.getHelpContentsCallCount_;
  }

  /**
   * Sets the value that will be returned when calling getHelpContents().
   * @param {!SearchResponse} response
   */
  setFakeSearchResponse(response) {
    this.methods_.setResult('getHelpContents', {response: response});
  }
}
