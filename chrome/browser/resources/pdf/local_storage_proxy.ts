// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper object to access localStorage. This proxy is useful for testing.
export interface LocalStorageProxy {
  getItem(key: string): string|null;
  setItem(key: string, value: string): void;
}

export class LocalStorageProxyImpl implements LocalStorageProxy {
  getItem(key: string): string|null {
    return window.localStorage ? window.localStorage.getItem(key) : null;
  }

  setItem(key: string, value: string): void {
    if (window.localStorage) {
      window.localStorage.setItem(key, value);
    }
  }

  static getInstance(): LocalStorageProxy {
    return instance || (instance = new LocalStorageProxyImpl());
  }

  static setInstance(obj: LocalStorageProxy): void {
    instance = obj;
  }
}

let instance: LocalStorageProxy|null = null;
