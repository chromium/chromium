// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



/**
 * The types of commands that can be sent between the offscreen document and the
 * ChromeVox service worker.
 */
export enum OffscreenCommandType {
  ON_KEY_DOWN = 'onKeyDown',
  ON_KEY_UP = 'onKeyUp',
  ON_CLIPBOARD_DATA_CHANGED = 'onClipboardDataChanged',
}
