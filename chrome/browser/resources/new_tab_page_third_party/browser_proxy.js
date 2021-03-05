// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';

import './new_tab_page_third_party.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class BrowserProxy {
  constructor() {
    /** @type {newTabPageThirdParty.mojom.PageCallbackRouter} */
    this.callbackRouter = new newTabPageThirdParty.mojom.PageCallbackRouter();

    /** @type {newTabPageThirdParty.mojom.PageHandlerRemote} */
    this.handler = new newTabPageThirdParty.mojom.PageHandlerRemote();

    const factory = newTabPageThirdParty.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /**
   * @param {string} query
   * @return {!MediaQueryList}
   */
  matchMedia(query) {
    return window.matchMedia(query);
  }

  /** @return {number} */
  now() {
    return Date.now();
  }
}

addSingletonGetter(BrowserProxy);
