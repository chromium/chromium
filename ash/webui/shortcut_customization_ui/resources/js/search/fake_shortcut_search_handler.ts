// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {MojoSearchResult} from '../shortcut_types';
import {ShortcutSearchHandlerInterface} from '../shortcut_types.js';

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

  // Stub search function.
  search(): Promise<{results: MojoSearchResult[]}> {
    return this.methods.resolveMethod('search');
  }

  /**
   * Sets the value that will be returned when calling search().
   */
  setFakeSearchResult(results: MojoSearchResult[]): void {
    this.methods.setResult('search', {results});
  }
}
