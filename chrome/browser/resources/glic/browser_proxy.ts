// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

import {GlicPreloadHandlerFactory, GlicPreloadHandlerRemote, PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, PreloadPageCallbackRouter} from './glic.mojom-webui.js';
import type {GlicPreloadHandlerInterface, PageHandlerInterface} from './glic.mojom-webui.js';

export interface BrowserProxy {
  pageHandler: PageHandlerInterface;
  glicPreloadHandler?: GlicPreloadHandlerInterface;
}

// Whether to enable PageHandler debug logging. Can be enabled with the
// --enable-features=GlicDebugWebview command-line flag.
const kEnableDebug = loadTimeData.getBoolean('enableDebug');
const kGlicWebContentsWarming =
    loadTimeData.getBoolean('glicWebContentsWarming');

export class BrowserProxyImpl implements BrowserProxy {
  pageHandler: PageHandlerRemote;
  pageCallbackRouter: PageCallbackRouter;

  glicPreloadHandler?: GlicPreloadHandlerRemote;
  preloadPageCallbackRouter: PreloadPageCallbackRouter;

  constructor() {
    this.pageCallbackRouter = new PageCallbackRouter();
    this.preloadPageCallbackRouter = new PreloadPageCallbackRouter();

    const pageHandlerRemote = new PageHandlerRemote();
    if (!kEnableDebug) {
      this.pageHandler = pageHandlerRemote;
    } else {
      this.pageHandler = new Proxy(pageHandlerRemote, {
        get(target: PageHandlerRemote, name: keyof PageHandlerRemote,
            receiver) {
          const prop = Reflect.get(target, name, receiver);
          if (!target.hasOwnProperty(name)) {
            if (typeof prop === 'function') {
              return function(this: PageHandlerRemote, ...args: any) {
                /* eslint no-console: ["error", { allow: ["log"] }] */
                console.log('PageHandler#', name, args);
                return (prop as any).apply(this, args);
              };
            }
          }
          return prop;
        },
      });
    }
    PageHandlerFactory.getRemote().createPageHandler(
        this.pageHandler.$.bindNewPipeAndPassReceiver(),
        this.pageCallbackRouter.$.bindNewPipeAndPassRemote());

    if (kGlicWebContentsWarming) {
      const preloadHandlerRemote = new GlicPreloadHandlerRemote();
      this.glicPreloadHandler = preloadHandlerRemote;
      GlicPreloadHandlerFactory.getRemote().createPreloadHandler(
          this.glicPreloadHandler.$.bindNewPipeAndPassReceiver(),
          this.preloadPageCallbackRouter.$.bindNewPipeAndPassRemote());
    }
  }
}
