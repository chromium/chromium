// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal version of load_time_data.js for chrome-untrusted://
 * origins. They are sandboxed, so cannot use chrome://resources ("unable to
 * load local resource") which load_time_data.js relies on through strings.js.
 * Since we don't want to maintain a "mirror" of all the module dependencies on
 * each chrome-untrusted:// origin. For simplicity, this version lacks all the
 * validation done by load_time_data.js, and just aims to provide a compatible
 * API.
 */

interface LoadTimeDataRaw {
  [key: string]: any;
}

class LoadTimeData {
  private data_: LoadTimeDataRaw = {};

  /**
   * Needs to be separate from data since some tast tests expect a data_ field.
   * TODO(b/192977700): Remove this.
   */
  set data(value: LoadTimeDataRaw) {
    this.data_ = value;
  }

  getValue(id: string): any {
    return this.data_[id];
  }

  getString(id: string): string {
    return this.data_[id];
  }

  getBoolean(id: string): boolean {
    return this.data_[id];
  }

  getInteger(id: string): number {
    return this.data_[id];
  }

  valueExists(id: string): boolean {
    return this.data_[id] !== undefined;
  }
}

const loadTimeData = new LoadTimeData();
(window as any).loadTimeData = loadTimeData;
