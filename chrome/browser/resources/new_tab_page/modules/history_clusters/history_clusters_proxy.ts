// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandler, PageHandlerRemote} from '../../history_clusters.mojom-webui.js';

export interface HistoryClustersProxy {
  handler: PageHandlerRemote;
}

export class HistoryClustersProxyImpl implements HistoryClustersProxy {
  handler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): HistoryClustersProxy {
    if (instance) {
      return instance;
    }

    const handler = PageHandler.getRemote();
    return instance = new HistoryClustersProxyImpl(handler);
  }

  static setInstance(obj: HistoryClustersProxy) {
    instance = obj;
  }
}

let instance: HistoryClustersProxy|null = null;
