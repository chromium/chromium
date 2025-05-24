// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PriceInsightsHandlerFactory, PriceInsightsHandlerRemote} from './price_insights.mojom-webui.js';

let instance: PriceInsightsBrowserProxy|null = null;

export interface PriceInsightsBrowserProxy {
  showSidePanelUi(): void;
  showFeedback(): void;
}

export class PriceInsightsBrowserProxyImpl implements
    PriceInsightsBrowserProxy {
  handler: PriceInsightsHandlerRemote;

  constructor() {
    this.handler = new PriceInsightsHandlerRemote();

    const factory = PriceInsightsHandlerFactory.getRemote();
    factory.createPriceInsightsHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  showSidePanelUi(): void {
    this.handler.showSidePanelUI();
  }

  showFeedback(): void {
    this.handler.showFeedback();
  }

  static getInstance(): PriceInsightsBrowserProxy {
    return instance || (instance = new PriceInsightsBrowserProxyImpl());
  }

  static setInstance(proxy: PriceInsightsBrowserProxy) {
    instance = proxy;
  }
}
