// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface Decision {
  action: 'stay'|'go';
  matching_rule?: string;
  reason: 'globally_disabled'|'protocol'|'sitelist'|'greylist'|'default';
}

/**
 * Returned by getRulesetSources().
 */
export interface RulesetSources {
  browser_switcher: {[k: string]: string};
}

/**
 * Returned by getTimestamps().
 */
export interface TimestampPair {
  last_fetch: number;
  next_fetch: number;
}

export interface RuleSetList {
  gpo: RuleSet;
  ieem?: RuleSet;
  external_sitelist?: RuleSet;
  external_greylist?: RuleSet;
}

export interface RuleSet {
  sitelist: string[];
  greylist: string[];
}

/** @interface */
export interface BrowserSwitchInternalsProxy {
  /**
   * Query whether the LBS feature is enabled by BrowserSwitcherEnabled policy
   */
  isBrowserSwitcherEnabled(): Promise<boolean>;

  // TODO(crbug.com/40200942): Add documentation.
  getDecision(url: string): Promise<Decision>;

  // TODO(crbug.com/40200942): Add documentation.
  getAllRulesets(): Promise<RuleSetList>;

  // TODO(crbug.com/40200942): Add documentation.
  getTimestamps(): Promise<TimestampPair>;

  // TODO(crbug.com/40200942): Add documentation.
  getRulesetSources(): Promise<RulesetSources>;

  // TODO(crbug.com/40200942): Add documentation.
  refreshXml(): void;
}

export class BrowserSwitchInternalsProxyImpl implements
    BrowserSwitchInternalsProxy {
  isBrowserSwitcherEnabled() {
    return sendWithPromise('isBrowserSwitcherEnabled');
  }

  getDecision(url: string) {
    return sendWithPromise('getDecision', url);
  }

  getAllRulesets(): Promise<RuleSetList> {
    return sendWithPromise('getAllRulesets');
  }

  getTimestamps(): Promise<TimestampPair> {
    return sendWithPromise('getTimestamps');
  }

  getRulesetSources(): Promise<RulesetSources> {
    return sendWithPromise('getRulesetSources');
  }

  refreshXml() {
    chrome.send('refreshXml');
  }

  static getInstance(): BrowserSwitchInternalsProxy {
    return instance || (instance = new BrowserSwitchInternalsProxyImpl());
  }
}

let instance: BrowserSwitchInternalsProxy|null = null;
