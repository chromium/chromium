// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const PRODUCT_NAME = 'ChromeOS_MediaApp';

interface ErrorObject {
  lineNumber?: number;
  columnNumber?: number;
  stack?: string;
  name?: string;
}

/**
 * Handles reporting errors creating a new Error() to get a stacktrace. If > 1
 * arguments are provided, builds up the error message using the other [1, n-1]
 * errors and appends that to the first error.
 */
function reportCrashError(...errors: any[]) {
  if (errors.length === 0) {
    return;
  }

  // Build up the error message using errors[1, n-1].
  let message = '';
  for (let i = 1; i < errors.length; i++) {
    const errorArg = errors[i];
    if (errorArg !== undefined) {
      if (errorArg instanceof Error) {
        message += `\n${errorArg.name}: ${errorArg.message}`;
      } else if (typeof errorArg === 'string') {
        message += ', ' + errorArg;
      } else {
        try {
          message += '\n' + JSON.stringify(errorArg);
        } catch (e) {
          message += '<object loop?>';
        }
      }
    }
  }

  // Base the error on the first error.
  let firstError = errors[0];
  let errorObject: ErrorObject|undefined = undefined;
  let errorMessage: string;
  let prefix = '';

  // Parse out the reason for the error.
  if (firstError instanceof PromiseRejectionEvent) {
    prefix = 'Unhandled rejection: ';
    firstError = firstError.reason;
  } else if (
      typeof firstError === 'object' && firstError !== null &&
      firstError.constructor) {
    prefix = firstError.constructor.name + ': ';
  }

  const maybeError: ErrorObject = firstError;
  if (firstError instanceof Error || firstError instanceof ErrorEvent ||
      firstError instanceof DOMException) {
    // Note: `ErrorEvent` doesn't have a name field, `DOMException`s are routed
    // through 'onerror' and treated as `ErrorEvents`, we can also have
    // `DOMExceptions` inside a unhandled rejection.
    errorMessage = `[${maybeError.name ?? ''}] ${firstError.message}`;
    errorObject = maybeError;

    // Events and exceptions won't have stacks. Make one.
    if (!errorObject?.stack) {
      errorObject.stack = new Error().stack;
    }
  } else {
    // Should just be a regular object.
    try {
      errorMessage = `Unexpected: ${JSON.stringify(firstError)}`;
    } catch (e) {
      errorMessage = `Unexpected: <object loop?>`;
    }
  }

  if (!errorObject) {
    // Create a new error to get a stacktrace.
    errorObject = new Error();
  }

  const params: chrome.crashReportPrivate.ErrorInfo = {
    product: PRODUCT_NAME,
    url: self.location.href,
    message: prefix + errorMessage + message,
    lineNumber: errorObject?.lineNumber || 0,
    stackTrace: errorObject?.stack || '',
    columnNumber: errorObject?.columnNumber || 0,
  };

  // TODO(crbug.com/40641337): Add useful callback when the error is reported,
  // handle if it crashes while reporting an error.
  chrome.crashReportPrivate.reportError(params, () => {});
}

/**
 * Redirect calls to `console.error` to also call `report` so we can report
 * console.errors. Pass `realConsoleError` into this so we can mock it out for
 * testing.
 */
function captureConsoleErrors(
    realConsoleError: (...rest: any[]) => any,
    report: (...rest: any[]) => any) {
  console.error = (...errors: any[]) => {
    // Still call real console.error.
    realConsoleError(...errors);
    // Send to error reporter.
    report(...errors, '(from console)');
  };
}

captureConsoleErrors(console.error, reportCrashError);

window.addEventListener('error', reportCrashError);
window.addEventListener('unhandledrejection', reportCrashError);

export const TEST_ONLY = {
  reportCrashError,
  captureConsoleErrors,
};
