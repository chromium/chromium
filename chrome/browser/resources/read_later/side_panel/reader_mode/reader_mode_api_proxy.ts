// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';

import {PageHandlerFactory, PageHandlerRemote} from './reader_mode.mojom-webui.js';

let instance: ReaderModeApiProxy|null = null;

export class ReaderModeApiProxy {
  handler: PageHandlerRemote;

  constructor() {
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  showReaderMode() {
    return this.handler.showReaderMode();
  }

  static getInstance() {
    return instance || (instance = new ReaderModeApiProxy());
  }

  static setInstance(obj: ReaderModeApiProxy) {
    instance = obj;
  }
}
