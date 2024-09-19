// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      logs_: {
        type: Array,
        value: [],
      },
    };
  }

  private browserProxy_: BrowserProxy|null = null;
  private logs_: string[] = [];

  constructor() {
    super();
    this.browserProxy_ = BrowserProxy.getInstance();
  }

  private async getCampaignsLogs(): Promise<{logs: string[]}> {
    return await this.browserProxy_!.handler!.getCampaignsLogs();
  }

  private async onClickRefreshButton_(event: Event) {
    event.stopPropagation();
    this.set('logs_', []);
    this.logs_ = this.logs_.concat((await this.getCampaignsLogs()).logs);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GrowthInternalsAppElement.is]: GrowthInternalsAppElement;
  }
}

customElements.define(GrowthInternalsAppElement.is, GrowthInternalsAppElement);
