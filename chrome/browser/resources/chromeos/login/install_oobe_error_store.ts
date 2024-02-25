// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export {};

declare global {
  interface Window {
    OobeErrorStore: OobeErrorStore;
  }
}

class OobeErrorStore {
  private static instance: OobeErrorStore;
  private store: ErrorEvent[];

  static getInstance(): OobeErrorStore {
    return OobeErrorStore.instance ||
        (OobeErrorStore.instance = new OobeErrorStore());
  }

  private constructor() {
    this.store = [];
    window.addEventListener('error', (e: ErrorEvent) => {
      // Add to the error store. This is used by tests that ensure no errors
      // are present by checking the length of this array.
      this.store.push(e);

      // Additionally, print out the error with its stack information on the
      // console so that it appears in log files.
      if (e.error && e.error.stack) {
        console.error(e.error.stack);
      }
    });
  }

  get length(): number {
    return this.store.length;
  }
}

window.OobeErrorStore = OobeErrorStore.getInstance();
