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

export class FakeHelpContentProvider implements HelpContentProviderInterface {
  /**
   * Record the last query passed to getHelpContents to help verify the method
   * has been called.
   */
  private lastQuery: string = '';
  /** Keep track of how many times the getHelpContents has been called.*/
  private getHelpContentsMethodCallCount: number = 0;
  private methods: FakeMethodResolver;

  constructor() {
    this.methods = new FakeMethodResolver();
    // Setup method resolvers.
    this.methods.register('getHelpContents');
  }

  getLastQuery(): string {
    return this.lastQuery;
  }

  getHelpContents(request: SearchRequest): Promise<{response: SearchResponse}> {
    ++this.getHelpContentsMethodCallCount;
    this.lastQuery = mojoString16ToString(request.query);
    return this.methods.resolveMethod('getHelpContents');
  }

  getHelpContentsCallCount(): number {
    return this.getHelpContentsMethodCallCount;
  }

  /** Sets the value that will be returned when calling getHelpContents().*/
  setFakeSearchResponse(response: SearchResponse) {
    this.methods.setResult('getHelpContents', {response});
  }
}
