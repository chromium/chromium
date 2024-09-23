// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let instance: WindowProxy|null = null;

/** Abstracts some builtin JS functions to mock them in tests. */
export class WindowProxy {
  static getInstance(): WindowProxy {
    return instance || (instance = new WindowProxy());
  }

  static setInstance(newInstance: WindowProxy) {
    instance = newInstance;
  }

  get onLine(): boolean {
    return window.navigator.onLine;
  }
}
