// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class TitleItem {
  title: string;
  expandable: boolean;
  expanded: boolean;

  constructor(
      title: string, expandable: boolean = false, expanded: boolean = false) {
    this.title = title;
    this.expandable = expandable;
    this.expanded = expanded;
  }
}
