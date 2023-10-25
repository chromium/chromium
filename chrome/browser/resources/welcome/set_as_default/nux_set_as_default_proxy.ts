// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {DefaultBrowserInfo} from '../shared/nux_types.js';

const NUX_SET_AS_DEFAULT_INTERACTION_METRIC_NAME =
    'FirstRun.NewUserExperience.SetAsDefaultInteraction';

enum NuxSetAsDefaultInteractions {
  PAGE_SHOWN = 0,
  NAVIGATED_AWAY,
  SKIP,
  CLICK_SET_DEFAULT,
  SUCCESSFULLY_SET_DEFAULT,
  NAVIGATED_AWAY_THROUGH_BROWSER_HISTORY,
}

const NUX_SET_AS_DEFAULT_INTERACTIONS_COUNT =
    Object.keys(NuxSetAsDefaultInteractions).length;

export interface NuxSetAsDefaultProxy {
  requestDefaultBrowserState(): Promise<DefaultBrowserInfo>;
  setAsDefault(): void;
  recordPageShown(): void;
  recordNavigatedAway(): void;
  recordNavigatedAwayThroughBrowserHistory(): void;
  recordSkip(): void;
  recordBeginSetDefault(): void;
  recordSuccessfullySetDefault(): void;
}

export class NuxSetAsDefaultProxyImpl implements NuxSetAsDefaultProxy {
  requestDefaultBrowserState() {
    return sendWithPromise('requestDefaultBrowserState');
  }

  setAsDefault() {
    chrome.send('setAsDefaultBrowser');
  }

  recordPageShown() {
    this.recordInteraction_(NuxSetAsDefaultInteractions.PAGE_SHOWN);
  }

  recordNavigatedAway() {
    this.recordInteraction_(NuxSetAsDefaultInteractions.NAVIGATED_AWAY);
  }

  recordNavigatedAwayThroughBrowserHistory() {
    this.recordInteraction_(
        NuxSetAsDefaultInteractions.NAVIGATED_AWAY_THROUGH_BROWSER_HISTORY);
  }

  recordSkip() {
    this.recordInteraction_(NuxSetAsDefaultInteractions.SKIP);
  }

  recordBeginSetDefault() {
    this.recordInteraction_(NuxSetAsDefaultInteractions.CLICK_SET_DEFAULT);
  }

  recordSuccessfullySetDefault() {
    this.recordInteraction_(
        NuxSetAsDefaultInteractions.SUCCESSFULLY_SET_DEFAULT);
  }

  private recordInteraction_(interaction: number): void {
    chrome.metricsPrivate.recordEnumerationValue(
        NUX_SET_AS_DEFAULT_INTERACTION_METRIC_NAME, interaction,
        NUX_SET_AS_DEFAULT_INTERACTIONS_COUNT);
  }

  static getInstance(): NuxSetAsDefaultProxy {
    return instance || (instance = new NuxSetAsDefaultProxyImpl());
  }

  static setInstance(obj: NuxSetAsDefaultProxy) {
    instance = obj;
  }
}

let instance: NuxSetAsDefaultProxy|null = null;
