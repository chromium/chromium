// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';

let instance: WindowProxy|null = null;
let lazyLoadPromise: Promise<void>|null = null;

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

  /**
   * Adds a <script> tag that holds the lazy loaded code, if not already
   * added, and returns a promise that resolves when it has loaded.
   */
  ensureLazyLoaded(): Promise<void> {
    if (!lazyLoadPromise) {
      const script = document.createElement('script');
      script.type = 'module';
      script.src = getTrustedScriptURL`./lazy_load.js`;
      document.body.appendChild(script);
      lazyLoadPromise = new Promise<void>(resolve => {
        script.onload = () => resolve();
      });
    }
    return lazyLoadPromise;
  }

  /**
   * Posts |message| on |target| at |targetOrigin| (or on its content window if
   * |target| is an iframe).
   */
  postMessage(
      target: HTMLIFrameElement|Window, message: unknown,
      targetOrigin: string) {
    const targetWindow =
        target instanceof HTMLIFrameElement ? target.contentWindow! : target;
    targetWindow.postMessage(message, targetOrigin);
  }

  /** Returns `window.location.href` wrapped in a URL object. */
  get url(): URL {
    return new URL(window.location.href);
  }

  get onLine(): boolean {
    return window.navigator.onLine;
  }
}
