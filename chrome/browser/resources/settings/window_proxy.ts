// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface WindowProxy {
  now(): number;
  getVisibilityState(): DocumentVisibilityState;
  hasFocus(): boolean;
}

export class WindowProxyImpl implements WindowProxy {
  now(): number {
    return performance.now();
  }

  getVisibilityState(): DocumentVisibilityState {
    return document.visibilityState;
  }

  hasFocus(): boolean {
    return document.hasFocus();
  }

  static getInstance(): WindowProxy {
    return instance || (instance = new WindowProxyImpl());
  }

  static setInstance(obj: WindowProxy) {
    instance = obj;
  }
}

let instance: WindowProxy|null = null;
