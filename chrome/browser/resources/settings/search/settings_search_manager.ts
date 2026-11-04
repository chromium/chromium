// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Sends Settings search queries to the browser-side SettingsSearchHandler
 * (behind the enableSettingsSearch2026 flag) and tracks the pending request.
 */

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import type {SearchResult} from '../settings_search.mojom-webui.js';
import {browserProxyFactory} from '../settings_search.mojom-webui.js';

export const MAX_SEARCH_RESULTS = 6;

const MIN_SEARCH_QUERY_LENGTH = 2;

export interface SettingsSearchRequest {
  query: string;
  results: SearchResult[];

  // Whether a newer search or clear() replaced this request before its
  // results arrived. Callers should ignore the results of canceled requests.
  canceled: boolean;
}

export interface SettingsSearchManager {
  /**
   * Requests results for `query`. Any pending request for a different query
   * is canceled. Queries shorter than MIN_SEARCH_QUERY_LENGTH (after
   * trimming) resolve with no results without querying the handler.
   * @return A promise that resolves with the request once its results arrive,
   *     or as soon as it is canceled.
   */
  search(query: string): Promise<SettingsSearchRequest>;

  /**
   * Cancels the pending request, if any.
   */
  clear(): void;
}

interface CurrentRequest {
  request: SettingsSearchRequest;
  resolver: PromiseResolver<SettingsSearchRequest>;
}

export class SettingsSearchManagerImpl implements SettingsSearchManager {
  private current_: CurrentRequest|null = null;

  search(query: string): Promise<SettingsSearchRequest> {
    // SettingsSearchHandler also trims, but the trimmed query is needed here
    // for the minimum length and duplicate query checks.
    const trimmedQuery = query.trim();
    if (this.current_ && this.current_.request.query === trimmedQuery) {
      return this.current_.resolver.promise;
    }

    this.clear();
    const request: SettingsSearchRequest = {
      query: trimmedQuery,
      results: [],
      canceled: false,
    };
    if (trimmedQuery.length < MIN_SEARCH_QUERY_LENGTH) {
      return Promise.resolve(request);
    }

    const resolver = new PromiseResolver<SettingsSearchRequest>();
    this.current_ = {request, resolver};
    this.sendRequest_(request, resolver);
    return resolver.promise;
  }

  clear() {
    if (!this.current_) {
      return;
    }

    const {request, resolver} = this.current_;
    this.current_ = null;
    if (!resolver.isFulfilled) {
      request.canceled = true;
      resolver.resolve(request);
    }
  }

  private async sendRequest_(
      request: SettingsSearchRequest,
      resolver: PromiseResolver<SettingsSearchRequest>): Promise<void> {
    try {
      const {results} = await browserProxyFactory.getInstance().handler.search(
          request.query, MAX_SEARCH_RESULTS);
      if (request.canceled) {
        return;
      }
      request.results = results;
      resolver.resolve(request);
    } catch {
      // The handler failed (for example, the Mojo pipe closed). Cancel the
      // request so that callers aren't left waiting and a repeated query sends
      // a new request.
      if (this.current_?.request === request) {
        this.clear();
      }
    }
  }

  static getInstance(): SettingsSearchManager {
    return instance || (instance = new SettingsSearchManagerImpl());
  }

  static setInstance(obj: SettingsSearchManager) {
    instance = obj;
  }

  static resetInstanceForTesting() {
    instance = null;
  }
}

let instance: SettingsSearchManager|null = null;
