// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class TitleItem {
  /**
   * @param {string} title
   * @param {boolean} expandable
   * @param {boolean} expanded
   */
  constructor(title, expandable = false, expanded = false) {
    /** @type {string} */
    this.title = title;

    /** @type {boolean} */
    this.expandable = expandable;

    /** @type {boolean} */
    this.expanded = expanded;
  }
}
