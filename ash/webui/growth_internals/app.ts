// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

export class GrowthInternalsAppElement extends PolymerElement {
  static get is() {
    return 'growth-internals-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      filteredLogs_: {
        type: Array,
        value: [],
      },
      campaignId_: {
        type: String,
        value: 0,
        notify: true,
      }
    };
  }

  private browserProxy_: BrowserProxy|null = null;
  private logs_: string[] = [];
  private filteredLogs_: string[] = [];
  private campaignId_ = '0';

  constructor() {
    super();
    this.browserProxy_ = BrowserProxy.getInstance();
  }

  private async getCampaignsLogs(): Promise<{logs: string[]}> {
    return await this.browserProxy_!.handler!.getCampaignsLogs();
  }

  private async onClickClearAllEventsButton_(event: Event) {
    event.stopPropagation();
    this.browserProxy_!.handler!.clearAllEvents();
  }

  private async onClickRefreshButton_(event: Event) {
    event.stopPropagation();
    this.set('logs_', []);
    this.logs_ = this.logs_.concat((await this.getCampaignsLogs()).logs);
    this.filterLogs_();
  }

  private onFilterByIdChange_(event: CustomEvent) {
    event.stopPropagation();
    this.filterLogs_();
  }

  private filterLogs_() {
    if (this.campaignId_ === '0') {
      // Show all the logs when `campaignId_` is set to `0`.
      this.filteredLogs_ = this.logs_;
      return;
    }

    this.filteredLogs_ = [];
    var startsMatchingCampaignWithId = false;
    const startIndicator = `Evaluating campaign: ${this.campaignId_}.`;
    const endIndicator = `Campaign: ${this.campaignId_} is matched:`;

    this.filteredLogs_ = this.logs_.filter((log: string) => {
      if (log.includes(startIndicator)) {
        startsMatchingCampaignWithId = true;
        return true;
      }

      if (log.includes(endIndicator)) {
        startsMatchingCampaignWithId = false;
        return true;
      }

      return startsMatchingCampaignWithId;
    });

    if (this.filteredLogs_.length === 0) {
      this.filteredLogs_ =
          [`No log is found for campaign ${this.campaignId_}.`];
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GrowthInternalsAppElement.is]: GrowthInternalsAppElement;
  }
}

customElements.define(GrowthInternalsAppElement.is, GrowthInternalsAppElement);
