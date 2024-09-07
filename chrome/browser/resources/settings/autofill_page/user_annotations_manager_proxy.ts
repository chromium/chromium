// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type UserAnnotationsEntry = chrome.autofillPrivate.UserAnnotationsEntry;

/**
 * This interface defines the autofill API wrapper that combines user
 * annotations related methods.
 */
export interface UserAnnotationsManagerProxy {
  /**
   * Returns user annotations entries.
   */
  getEntries(): Promise<UserAnnotationsEntry[]>;
}

export class UserAnnotationsManagerProxyImpl implements
    UserAnnotationsManagerProxy {
  getEntries(): Promise<UserAnnotationsEntry[]> {
    return chrome.autofillPrivate.getUserAnnotationsEntries();
  }

  static getInstance(): UserAnnotationsManagerProxy {
    return instance || (instance = new UserAnnotationsManagerProxyImpl());
  }

  static setInstance(obj: UserAnnotationsManagerProxy): void {
    instance = obj;
  }
}

let instance: UserAnnotationsManagerProxy|null = null;
