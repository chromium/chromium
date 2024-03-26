// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
export class Router {
  private static instance_: Router|null = null;

  static getInstance() {
    if (!this.instance_) {
      this.instance_ = new Router();
    }
    return this.instance_;
  }

  static setInstance(router: Router) {
    this.instance_ = router;
  }

  getCurrentQuery(): string {
    return window ? window.location.search : '';
  }
}
