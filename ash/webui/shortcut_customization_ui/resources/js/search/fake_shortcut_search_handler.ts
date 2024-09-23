// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {SearchResultsAvailabilityObserverRemote} from '../../mojom-webui/search.mojom-webui.js';
import {MojoSearchResult, ShortcutSearchHandlerInterface} from '../shortcut_types.js';

/**
 * @fileoverview
 * Implements a fake version of the ShortcutSearchHandler mojo interface.
 */
export class FakeShortcutSearchHandler implements
    ShortcutSearchHandlerInterface {
  private methods: FakeMethodResolver;

  constructor() {
    this.methods = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods.register('search');
  }

  // Add '_' to parameters to suppress unused-variable warning.
  search(_query: String16, _maxNumResult: number):
      Promise<{results: MojoSearchResult[]}> {
    return this.methods.resolveMethod('search');
  }

  addSearchResultsAvailabilityObserver(
      _observer: SearchResultsAvailabilityObserverRemote): void {
    // Intentionally not implemented.
  }

  /**
   * Sets the value that will be returned when calling search().
   */
  setFakeSearchResult(results: MojoSearchResult[]): void {
    this.methods.setResult('search', {results});
  }
}
