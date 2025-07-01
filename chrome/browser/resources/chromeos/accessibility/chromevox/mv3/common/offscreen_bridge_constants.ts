// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface StateWithMaxCellHeight {
  rows: number;
  columns: number;
  cellWidth: number;
  cellHeight: number;
  maxCellHeight: number;
}

export interface ClipboardData {
  eventType?: string, clipboardContent?: string,
}
