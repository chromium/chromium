// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides an abstraction layer for Window.open() for
 * for mocking in tests.
 * TODO(http://crbug.com/1250487): Refactor this file and similar files into
 * ui/webui/resources/js/
 */

export interface OpenWindowProxy {
  open(url: string): void;
}

export class OpenWindowProxyImpl implements OpenWindowProxy {
  open(url: string) {
    window.open(url);
  }

  static getInstance(): OpenWindowProxy {
    return instance || (instance = new OpenWindowProxyImpl());
  }

  static setInstance(obj: OpenWindowProxy) {
    instance = obj;
  }
}

let instance: OpenWindowProxy|null = null;
