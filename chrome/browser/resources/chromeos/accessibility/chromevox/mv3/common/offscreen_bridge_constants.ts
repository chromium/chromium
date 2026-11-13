// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeVoxMessageId} from './msgs.js';

export interface StateWithMaxCellHeight {
  rows: number;
  columns: number;
  cellWidth: number;
  cellHeight: number;
  maxCellHeight: number;
}

export interface ClipboardData {
  eventType?: ChromeVoxMessageId, clipboardContent?: string,
}
