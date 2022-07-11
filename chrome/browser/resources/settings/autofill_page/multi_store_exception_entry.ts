// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStoreExceptionEntry is used for showing exceptions that
 * are duplicated across stores as a single item in the UI.
 */

import {MultiStoreIdHandler} from './multi_store_id_handler.js';

/**
 * A version of chrome.passwordsPrivate.ExceptionEntry used for deduplicating
 * exceptions from the device and the account.
 */
export class MultiStoreExceptionEntry extends MultiStoreIdHandler {
  private urls_: chrome.passwordsPrivate.UrlCollection;

  constructor(entry: chrome.passwordsPrivate.ExceptionEntry) {
    super();

    this.urls_ = entry.urls;

    this.setId(entry.id, false);
  }

  get urls() {
    return this.urls_;
  }
}
