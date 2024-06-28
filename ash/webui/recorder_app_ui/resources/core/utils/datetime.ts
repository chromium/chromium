// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Date and time related utilities.
 */

export interface Duration {
  seconds?: number;
  milliseconds?: number;
}

function padZero(num: number): string {
  return num.toString().padStart(2, '0');
}

/**
 * A Intl.DurationFormat style API to convert duration into digital format.
 */
export function formatDuration(duration: Duration, digits = 0): string {
  // TODO(shik): Add unit test.
  // TODO(shik): Use Intl.DurationFormat when Chrome supports it.
  let secs = (duration.seconds ?? 0) + (duration.milliseconds ?? 0) / 1000;
  // Round the seconds to requested digits first, to prevent case like 119.6
  // seconds being formatted as "1:60".
  secs = Number(secs.toFixed(digits));
  let mins = Math.floor(secs / 60);
  secs %= 60;
  const hours = Math.floor(mins / 60);
  mins %= 60;

  // There's no '.' in `secsStr` when digits === 0.
  const secsStr =
    secs.toFixed(digits).padStart(digits === 0 ? 2 : 3 + digits, '0');

  if (hours > 0) {
    return `${hours}:${padZero(mins)}:${secsStr}`;
  } else {
    return `${padZero(mins)}:${secsStr}`;
  }
}

const DATE_FORMAT = new Intl.DateTimeFormat('en-US', {
  month: 'short',
  day: 'numeric',
});

/**
 * Formats the timestamp into a date string.
 *
 * TODO(pihsun): Handles i18n.
 *
 * @param timestamp Number of milliseconds elapsed since epoch.
 * @return The date string.
 * @example formatDate(975902640000) // => 'Dec 4'.
 */
export function formatDate(timestamp: number): string {
  return DATE_FORMAT.format(new Date(timestamp));
}

const TIME_FORMAT = new Intl.DateTimeFormat('en-US', {
  hour: 'numeric',
  minute: '2-digit',
  hour12: true,
});

/**
 * Formats the timestamp into a time string.
 *
 * TODO(pihsun): Handles i18n.
 *
 * @param timestamp Number of milliseconds elapsed since epoch.
 * @return The date string.
 * @example formatTime(975902640000) // => '12:04pm'.
 */
export function formatTime(timestamp: number): string {
  return TIME_FORMAT.format(new Date(timestamp)).toLowerCase().replace(' ', '');
}

/**
 * Returns the timestamp of today's date, which is today at exactly 12:00 AM.
 *
 * @return The timestamp of today's date.
 * @example getToday() => 1719244800000.
 */
export function getToday(): number {
  const today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(0);
  today.setMilliseconds(0);
  return today.getTime();
}

/**
 * Returns the timestamp of yesterday's date.
 *
 * @return The timestamp of yesterday's date.
 * @example getYesterday() => 1719158400000.
 */
export function getYesterday(): number {
  const dayInMilliseconds = 24 * 60 * 60 * 1000;
  const yesterday = new Date(getToday());
  return yesterday.getTime() - dayInMilliseconds;
}

const MONTH_FORMAT = new Intl.DateTimeFormat('en-US', {
  month: 'long',
  year: 'numeric',
});

/**
 * Format the timestamp into a string of month and year.
 *
 * TODO: b/336963138 - Handle i18n.
 *
 * @param timestamp Number of milliseconds elapsed since epoch.
 * @return The string containing month and year.
 * @example getMonthLabel(975902640000) => 'December 2000'.
 */
export function getMonthLabel(timestamp: number): string {
  return MONTH_FORMAT.format(new Date(timestamp));
}

/**
 * Returns whether the timestamp is in the current month.
 *
 * @param timestamp Number of milliseconds elapsed since epoch.
 * @return The boolean indicating if the timestamp is in the current month.
 * @example isInThisMonth(1719244800000) => true.
 */
export function isInThisMonth(timestamp: number): boolean {
  const now = new Date();
  const date = new Date(timestamp);
  return date.getMonth() === now.getMonth() &&
    date.getFullYear() === now.getFullYear();
}
