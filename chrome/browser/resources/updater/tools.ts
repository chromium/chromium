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
