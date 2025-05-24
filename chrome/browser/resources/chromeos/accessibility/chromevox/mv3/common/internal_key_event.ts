// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Service worker analog to KeyboardEvent for processing
 * KeyboardEvent data. Also matches EventLikeObject.
 */
export class InternalKeyEvent {
  type: string;
  keyCode: number;

  altKey?: boolean;
  ctrlKey?: boolean;
  metaKey?: boolean;
  shiftKey?: boolean;
  searchKeyHeld?: boolean;
  stickyMode?: boolean;
  repeat?: boolean;

  // Match key_sequence.ts EventLikeObject type
  keyPrefix?: boolean;
  prefixKey?: boolean;
  [key: string]: string|number|boolean|undefined;

  constructor(evt: any) {
    this.type = evt.type;
    this.keyCode = evt.keyCode;
    this.altKey = evt.altKey;
    this.ctrlKey = evt.ctrlKey;
    this.metaKey = evt.metaKey;
    this.shiftKey = evt.shiftKey;
    this.searchKeyHeld = evt.searchKeyHeld;
    this.stickyMode = evt.stickyMode;
    this.repeat = evt.repeat;
    this.keyPrefix = evt.keyPrefix;
    this.prefixKey = evt.prefixKey;
  }
}
