// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import * as metrics from './metrics.js';
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

/**
 * Converts v8 CallSite object to StackFrame.
 */
function toStackFrame(callsite: CallSite): StackFrame {
  // TODO(crbug.com/1072700): Handle native frame.
  let fileName = callsite.getFileName() ?? 'unknown';
  if (fileName.startsWith(window.location.origin)) {
    fileName = fileName.substring(window.location.origin.length + 1);
  }
  function ensureNumber(n: number|undefined) {
    return n === undefined ? -1 : n;
  }
  return {
    fileName,
    funcName: callsite.getFunctionName() ?? '[Anonymous]',
    lineNo: ensureNumber(callsite.getLineNumber()),
    colNo: ensureNumber(callsite.getColumnNumber()),
  };
}

function parseStackTrace(stackTrace: string): StackFrame[] {
  const regex = /at (\[?\w+\]? )?\(?(.+):(\d+):(\d+)/g;
  const frames: StackFrame[] = [];
  for (const m of stackTrace.matchAll(regex)) {
    frames.push({
      funcName: m[1]?.trim() ?? '',
      fileName: m[2],
      lineNo: Number(m[3]),
      colNo: Number(m[4]),
    });
  }
  return frames;
}

/**
 * Gets stack frames from error.
 *
 * @return Return null if failed to get frames from error.
 */
function getStackFrames(error: Error): StackFrame[] {
  const prevPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = (_error, stack) => {
    try {
      return stack.map(toStackFrame);
    } catch (e) {
      console.warn('Failed to prepareStackTrace', e);
      return [];
    }
  };

  let frames: StackFrame[];
  if (typeof error.stack === 'string') {
    // TODO(b/223324206): There is a known issue that when reporting error from
    // intent instance, the type from |error.stack| will be a string instead.
    frames = parseStackTrace(error.stack);
  } else {
    // Generally, error.stack returns whatever Error.prepareStackTrace returns.
    // Since we override Error.prepareStackTrace to return StackFrame[] here,
    // using "as unknown" first so that we can cast the type to StackFrame[].
    frames = error.stack as unknown as StackFrame[];
  }
  Error.prepareStackTrace = prevPrepareStackTrace;
  return frames;
}

/**
 * Gets the description text for an error.
 */
function getErrorDescription(error: Error): string {
  return `${error.name}: ${error.message}`;
}

/**
 * Gets formatted string stack from error.
 */
function formatErrorStack(error: Error, frames: StackFrame[]|null): string {
  const errorDesc = getErrorDescription(error);
  return errorDesc +
      (frames ?? [])
          .map(({fileName, funcName, lineNo, colNo}) => {
            let position = '';
            if (lineNo !== -1) {
              position = `:${lineNo}`;
              if (colNo !== -1) {
                position += `:${colNo}`;
              }
            }
            return `\n    at ${funcName} (${fileName}${position})`;
          })
          .join('');
}

const appWindow = window.appWindow;

/**
 * Initializes error collecting functions.
 */
export function initialize(): void {
  window.addEventListener('unhandledrejection', (e) => {
    reportError(
        ErrorType.UNCAUGHT_PROMISE, ErrorLevel.ERROR,
        assertInstanceof(e.reason, Error));
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
    type: ErrorType, level: ErrorLevel, errorRaw: unknown): void {
  const error = assertInstanceof(errorRaw, Error);
  // Uncaught promise is already logged in console.
  if (type !== ErrorType.UNCAUGHT_PROMISE) {
    if (level === ErrorLevel.ERROR) {
      console.error(type, error);
    } else if (level === ErrorLevel.WARNING) {
      console.warn(type, error);
    }
  }

  const time = Date.now();
  const frames = getStackFrames(error);
  const errorName = error.name;
  const errorDesc = getErrorDescription(error);
  const {fileName = '', lineNo = 0, colNo = 0, funcName = ''} =
      frames.length > 0 ? frames[0] : {};

  const hash = [errorName, fileName, String(lineNo), String(colNo)].join(',');
  if (triggeredErrorSet.has(hash)) {
    return;
  }
  triggeredErrorSet.add(hash);

  const stackStr = formatErrorStack(error, frames);
  if (appWindow !== null) {
    appWindow.reportError({
      type,
      level,
      stack: stackStr,
      time,
      name: errorName,
    });
    return;
  }
  metrics.sendErrorEvent({
    type,
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
    message: `${type}: ${errorDesc}`,
    lineNumber: lineNo || 0,
    stackTrace: stackStr || '',
    columnNumber: colNo || 0,
  };

  chrome.crashReportPrivate.reportError(
      params,
      () => {
          // Do nothing after error reported.
      });
}
