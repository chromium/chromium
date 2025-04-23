// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Debouncer {
  private duration_: number = 0;
  private timeoutId_: number = -1;

  constructor(duration: number) {
    this.duration_ = duration;
  }

  clear() {
    if (this.timeoutId_ !== -1) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = -1;
    }
  }

  call(fn: () => void) {
    this.clear();
    this.timeoutId_ = setTimeout(() => {
      this.timeoutId_ = -1;
      fn();
    }, this.duration_);
  }
}
