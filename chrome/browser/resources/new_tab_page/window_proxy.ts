// Copyright 2021 The Chromium Authors
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

  navigate(href: string) {
    window.location.href = href;
  }

  open(url: string) {
    window.open(url, '_blank');
  }

  setTimeout(callback: () => void, duration: number): number {
    return window.setTimeout(callback, duration);
  }

  clearTimeout(id: number|null) {
    window.clearTimeout(id !== null ? id : undefined);
  }

  random(): number {
    return Math.random();
  }

  createIframeSrc(src: string): string {
    return src;
  }

  matchMedia(query: string): MediaQueryList {
    return window.matchMedia(query);
  }

  now(): number {
    return Date.now();
  }

  /** Returns promise that resolves when lazy rendering should be started. */
  waitForLazyRender(): Promise<void> {
    return new Promise<void>(resolve => {
      requestIdleCallback(() => resolve(), {timeout: 500});
    });
  }

  /** Posts |message| on the content window of |iframe| at |targetOrigin|. */
  postMessage(iframe: HTMLIFrameElement, message: any, targetOrigin: string) {
    iframe.contentWindow!.postMessage(message, targetOrigin);
  }

  /** Returns `window.location.href` wrapped in a URL object. */
  get url(): URL {
    return new URL(window.location.href);
  }

  get onLine(): boolean {
    return window.navigator.onLine;
  }
}
