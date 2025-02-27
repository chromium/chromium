// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import {PageHandlerFactory, PageHandlerRemote, PageReceiver} from './glic.mojom-webui.js';
import type {PageHandlerInterface, PageInterface} from './glic.mojom-webui.js';

export interface BrowserProxy {
  handler: PageHandlerInterface;
}

// Whether to enable PageHandler debug logging. Can be enabled with the
// --enable-features=GlicDebugWebview command-line flag.
const kEnableDebug = loadTimeData.getBoolean('enableDebug');

export class BrowserProxyImpl implements BrowserProxy {
  handler: PageHandlerInterface;
  constructor(pageInterface: PageInterface) {
    const pageReceiver = new PageReceiver(pageInterface);
    const pageHandlerRemote = new PageHandlerRemote();
    this.handler = pageHandlerRemote;
    if (kEnableDebug) {
      this.handler = new Proxy(pageHandlerRemote, {
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
        pageHandlerRemote.$.bindNewPipeAndPassReceiver(),
        pageReceiver.$.bindNewPipeAndPassRemote());
  }
}
