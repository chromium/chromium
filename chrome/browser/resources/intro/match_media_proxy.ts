// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface MatchMediaProxy {
  matchMedia(query: string): MediaQueryList;
}

export class MatchMediaProxyImpl implements MatchMediaProxy {
  matchMedia(query: string): MediaQueryList {
    return window.matchMedia(query);
  }

  static getInstance(): MatchMediaProxy {
    return instance || (instance = new MatchMediaProxyImpl());
  }

  static setInstance(proxy: MatchMediaProxy) {
    instance = proxy;
  }
}

let instance: MatchMediaProxy|null = null;
