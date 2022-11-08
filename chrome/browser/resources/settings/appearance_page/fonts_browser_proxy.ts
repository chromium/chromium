// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface FontsData {
  fontList: Array<[string, string, string]>;
}

export interface FontsBrowserProxy {
  fetchFontsData(): Promise<FontsData>;
}

export class FontsBrowserProxyImpl implements FontsBrowserProxy {
  fetchFontsData() {
    return sendWithPromise('fetchFontsData');
  }

  static getInstance(): FontsBrowserProxy {
    return instance || (instance = new FontsBrowserProxyImpl());
  }

  static setInstance(obj: FontsBrowserProxy) {
    instance = obj;
  }
}

let instance: FontsBrowserProxy|null = null;
