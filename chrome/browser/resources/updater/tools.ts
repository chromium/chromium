// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const LONG_DATE_FORMATTER = new Intl.DateTimeFormat(undefined, {
  timeZoneName: 'short',
  month: 'numeric',
  day: 'numeric',
  hour: 'numeric',
  minute: 'numeric',
  second: 'numeric',
});

const SHORT_DATE_FORMATTER = new Intl.DateTimeFormat(undefined, {
  timeZoneName: 'short',
  month: 'numeric',
  day: 'numeric',
  hour: 'numeric',
  minute: 'numeric',
});

const DATE_FORMATTER_DIGITS = new Intl.DateTimeFormat(undefined, {
  timeZoneName: 'short',
  year: 'numeric',
  month: '2-digit',
  day: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
});

const DURATION_FORMATTER = new Intl.DurationFormat(undefined, {
  style: 'narrow',
});

/**
 * Formats a Date object into a human-readable localized string with second
 * precision.
 */
export function formatDateLong(date: Date): string {
  return LONG_DATE_FORMATTER.format(date);
}

/**
 * Formats a Date object into a human-readable localized string with minute
 * precision.
 */
export function formatDateShort(date: Date): string {
  return SHORT_DATE_FORMATTER.format(date);
}

/**
 * Formats a Date object into a human-readable localized string with second
 * precision in the '2-digit' format (e.g. "01/13/2026, 12:27 PM PST").
 */
export function formatDateDigits(date: Date): string {
  return DATE_FORMATTER_DIGITS.format(date);
}

/**
 * Formats a duration into a human-readable localized string with second
 * precision.
 */
export function formatDuration(
    days: number, hours: number, minutes: number, seconds: number): string {
  return DURATION_FORMATTER.format({days, hours, minutes, seconds});
}

/**
 * Formats a Date object into a human-readable localized string describing the
 * relative difference between that date and now.
 */
export function formatRelativeDate(date: Date): string {
  const now = new Date();
  const diffInSeconds = (now.getTime() - date.getTime()) / 1000;
  const rtf = new Intl.RelativeTimeFormat();

  if (diffInSeconds < 60) {
    return rtf.format(-Math.floor(diffInSeconds), 'second');
  }
  const diffInMinutes = diffInSeconds / 60;
  if (diffInMinutes < 60) {
    return rtf.format(-Math.floor(diffInMinutes), 'minute');
  }
  const diffInHours = diffInMinutes / 60;
  if (diffInHours < 24) {
    return rtf.format(-Math.floor(diffInHours), 'hour');
  }
  const diffInDays = diffInHours / 24;
  return rtf.format(-Math.floor(diffInDays), 'day');
}

/**
 * Checks whether two values are recursively equal. Only compares serializable
 * data (primitives, serializable arrays and serializable objects).
 * @param val1 Value to compare.
 * @param val2 Value to compare with val1.
 * @return Whether the values are recursively equal.
 */
export function deepEqual(val1: any, val2: any): boolean {
  if (val1 === val2) {
    return true;
  }

  if (Array.isArray(val1) || Array.isArray(val2)) {
    if (!Array.isArray(val1) || !Array.isArray(val2)) {
      return false;
    }
    return arraysEqual(val1, val2);
  }

  if (val1 instanceof Object && val2 instanceof Object) {
    return objectsEqual(val1, val2);
  }

  return false;
}

/**
 * @return Whether the arrays are recursively equal.
 */
function arraysEqual(arr1: any[], arr2: any[]): boolean {
  if (arr1.length !== arr2.length) {
    return false;
  }

  for (let i = 0; i < arr1.length; i++) {
    if (!deepEqual(arr1[i], arr2[i])) {
      return false;
    }
  }

  return true;
}

/**
 * @return Whether the objects are recursively equal.
 */
function objectsEqual(
    obj1: {[key: string]: any}, obj2: {[key: string]: any}): boolean {
  const keys1 = Object.keys(obj1);
  const keys2 = Object.keys(obj2);
  if (keys1.length !== keys2.length) {
    return false;
  }

  return keys1.every(key => deepEqual(obj1[key], obj2[key]));
}
