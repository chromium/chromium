// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote} from '../../tab_groups.mojom-webui.js';
import {PageHandler} from '../../tab_groups.mojom-webui.js';

export interface TabGroupsProxy {
  handler: PageHandlerRemote;
}

export class TabGroupsProxyImpl implements TabGroupsProxy {
  handler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): TabGroupsProxy {
    return instance ||
        (instance = new TabGroupsProxyImpl(PageHandler.getRemote()));
  }

  static setInstance(newInstance: TabGroupsProxy) {
    instance = newInstance;
  }
}

let instance: TabGroupsProxy|null = null;
