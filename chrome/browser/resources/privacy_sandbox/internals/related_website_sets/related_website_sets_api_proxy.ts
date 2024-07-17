// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {RelatedWebsiteSetsPageHandlerInterface} from './related_website_sets.mojom-webui.js';
import {RelatedWebsiteSetsPageHandler} from './related_website_sets.mojom-webui.js';

// Exporting the interface helps when creating a TestBrowserProxy wrapper.
export interface RelatedWebsiteSetsApiBrowserProxy {
  handler: RelatedWebsiteSetsPageHandlerInterface;
}

export class RelatedWebsiteSetsApiBrowserProxyImpl implements
    RelatedWebsiteSetsApiBrowserProxy {
  handler: RelatedWebsiteSetsPageHandlerInterface =
      RelatedWebsiteSetsPageHandler.getRemote();

  private constructor() {}

  static getInstance(): RelatedWebsiteSetsApiBrowserProxy {
    return instance || (instance = new RelatedWebsiteSetsApiBrowserProxyImpl());
  }

  static setInstance(proxy: RelatedWebsiteSetsApiBrowserProxy) {
    instance = proxy;
  }
}

let instance: RelatedWebsiteSetsApiBrowserProxy|null = null;
