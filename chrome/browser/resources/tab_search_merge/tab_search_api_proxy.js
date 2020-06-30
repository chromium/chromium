// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './tab_search.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class TabSearchApiProxy {
  constructor() {
    /** @type {!tabSearch.mojom.PageCallbackRouter} */
    this.callbackRouter = new tabSearch.mojom.PageCallbackRouter();

    /** @type {!tabSearch.mojom.PageHandlerRemote} */
    this.handler = new tabSearch.mojom.PageHandlerRemote();

    const factory = tabSearch.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /**  @return {Promise<{profileTabs: tabSearch.mojom.ProfileTabs}>} */
  getProfileTabs() {
    return this.handler.getProfileTabs();
  }

  /** @param {!tabSearch.mojom.SwitchToTabInfo} info */
  switchToTab(info) {
    this.handler.switchToTab(info);
  }
}

addSingletonGetter(TabSearchApiProxy);
