// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// LogLevel values must match the strings provided by the backend.
// LINT.IfChange
export enum LogLevel {
  DEBUG = 'Debug',
  EVENT = 'Event',
  USER = 'User',
  ERROR = 'Error',
}
// LINT.ThenChange(/components/device_event_log/device_event_log_impl.cc)

export interface LogEntry {
  event: string;
  file: string;
  level: LogLevel;
  timestampshort: string;
  timestamp: string;
  type: string;
}

export interface BrowserProxy {
  getLog(): Promise<string>;
  clearLog(): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  getLog(): Promise<string> {
    return sendWithPromise('getLog');
  }

  clearLog() {
    chrome.send('clearLog');
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
let instance: BrowserProxy|null = null;
