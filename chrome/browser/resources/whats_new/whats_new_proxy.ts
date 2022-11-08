// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface WhatsNewProxy {
  initialize(): Promise<string>;
}

export class WhatsNewProxyImpl implements WhatsNewProxy {
  initialize(): Promise<string> {
    return sendWithPromise('initialize');
  }

  static getInstance(): WhatsNewProxy {
    return instance || (instance = new WhatsNewProxyImpl());
  }

  static setInstance(obj: WhatsNewProxy) {
    instance = obj;
  }
}

let instance: WhatsNewProxy|null = null;
