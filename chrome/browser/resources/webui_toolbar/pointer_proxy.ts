// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PointerProxy {
  setPointerCapture(el: Element, pointerId: number): void;
  releasePointerCapture(el: Element, pointerId: number): void;
}

export class PointerProxyImpl implements PointerProxy {
  setPointerCapture(el: Element, pointerId: number) {
    el.setPointerCapture(pointerId);
  }

  releasePointerCapture(el: Element, pointerId: number) {
    el.releasePointerCapture(pointerId);
  }

  static getInstance(): PointerProxy {
    return instance || (instance = new PointerProxyImpl());
  }

  static setInstance(obj: PointerProxy) {
    instance = obj;
  }
}

let instance: PointerProxy|null = null;
