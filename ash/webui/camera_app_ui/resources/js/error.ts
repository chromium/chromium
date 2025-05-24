// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import * as metrics from './metrics.js';
import {isLocalDev} from './models/load_time_data.js';
import {
  ErrorLevel,
  ErrorType,
} from './type.js';

/**
 * Code location of stack frame.
 */
export interface StackFrame {
  fileName: string;
  funcName: string;
  lineNo: number;
  colNo: number;
}

const PRODUCT_NAME = 'ChromeOS_CameraApp';

function parseTopFrameInfo(stackTrace: string): StackFrame {
  const regex = /at (\[?\w+\]? |)\(?(.+):(\d+):(\d+)/;
  const match = regex.exec(stackTrace) ?? ['', '', '', '-1', '-1'] as const;
  return {
    funcName: match[1].trim(),
    fileName: match[2],
    lineNo: Number(match[3]),
    colNo: Number(match[4]),
  };
}

/**
 * Initializes error collecting functions.
 */
export function initialize(): void {
  window.addEventListener('unhandledrejection', (e) => {
    reportError(ErrorType.UNCAUGHT_PROMISE, ErrorLevel.ERROR, e.reason);
  });
  window.addEventListener('error', (e) => {
    reportError(ErrorType.UNCAUGHT_ERROR, ErrorLevel.ERROR, e.error);
  });
}

/**
 * All triggered error will be hashed and saved in this set to prevent the same
 * error being triggered multiple times.
 */
const triggeredErrorSet = new Set<string>();

/**
 * Reports error either through test error callback in test run or to error
 * metrics in non test run.
 */
export function reportError(
    errorType: ErrorType, level: ErrorLevel, errorRaw: unknown): void {
  const error = assertInstanceof(errorRaw, Error);
  // Uncaught errors will be logged to the console by browser.
  if (![ErrorType.UNCAUGHT_ERROR, ErrorType.UNCAUGHT_PROMISE].includes(
          errorType)) {
    if (level === ErrorLevel.ERROR) {
      console.error(errorType, error);
    } else if (level === ErrorLevel.WARNING) {
      console.warn(errorType, error);
    }
  }

  const time = Date.now();
  const errorName = error.name;
  const stackStr = error.stack ?? '';
  const {fileName, lineNo, colNo, funcName} = parseTopFrameInfo(stackStr);

  const hash = `${errorName},${fileName},${lineNo},${colNo}`;
  if (triggeredErrorSet.has(hash)) {
    return;
  }
  triggeredErrorSet.add(hash);

  if (window.appWindow !== null) {
    void window.appWindow.reportError({
      type: errorType,
      level,
      stack: stackStr,
      time,
      name: errorName,
    });
    return;
  }
  metrics.sendErrorEvent({
    type: errorType,
    level,
    errorName,
    fileName,
    funcName,
    lineNo: String(lineNo),
    colNo: String(colNo),
  });

  // Only reports the error to crash server if it reaches "error" level.
  if (level !== ErrorLevel.ERROR) {
    return;
  }

  const params = {
    product: PRODUCT_NAME,
    url: self.location.href,
    message: `${errorType}: ${errorName}: ${error.message}`,
    lineNumber: lineNo,
    stackTrace: stackStr,
    columnNumber: colNo,
  };

  if (isLocalDev()) {
    console.info('crashReportPrivate called with:', params);
  } else {
    chrome.crashReportPrivate.reportError(
        params,
        () => {
            // Do nothing after error reported.
        });
  }
}
