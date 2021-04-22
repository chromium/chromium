// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr', function() {
  class ErrorStore {
    constructor() {
      this.store_ = [];
      window.addEventListener('error', (e) => {
        this.store_.push(e);
      });
    }

    get length() {
      return this.store_.length;
    }
  }

  cr.addSingletonGetter(ErrorStore);

  return {
    ErrorStore: ErrorStore,
  };
});

cr.ErrorStore.getInstance();
