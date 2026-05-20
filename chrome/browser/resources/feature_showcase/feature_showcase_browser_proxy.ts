// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FeatureShowcasePageHandlerInterface} from './feature_showcase.mojom-webui.js';
import {FeatureShowcasePageHandlerFactory, FeatureShowcasePageHandlerRemote} from './feature_showcase.mojom-webui.js';

export interface FeatureShowcaseBrowserProxy {
  handler: FeatureShowcasePageHandlerInterface;
}

export class FeatureShowcaseBrowserProxyImpl implements
    FeatureShowcaseBrowserProxy {
  handler: FeatureShowcasePageHandlerInterface;

  private constructor() {
    this.handler = new FeatureShowcasePageHandlerRemote();
    FeatureShowcasePageHandlerFactory.getRemote().createPageHandler(
        (this.handler as FeatureShowcasePageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): FeatureShowcaseBrowserProxy {
    return instance || (instance = new FeatureShowcaseBrowserProxyImpl());
  }

  static setInstance(proxy: FeatureShowcaseBrowserProxy) {
    instance = proxy;
  }
}

let instance: FeatureShowcaseBrowserProxy|null = null;
