// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1234307): Delete this file once prefs_behavior.js has been
// migrated to TypeScript.

export interface PrefsBehaviorInterface {
  getPref(prefPath: string): chrome.settingsPrivate.PrefObject;
  setPrefValue(prefPath: string, value: any): void;
  appendPrefListItem(key: string, item: any): void;
  deletePrefListItem(key: string, item: any): void;
}
