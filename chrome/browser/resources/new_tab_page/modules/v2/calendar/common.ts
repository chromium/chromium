// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;

/**
 * Calendar actions. This enum must match the numbering for
 * NTPCalendarAction in enums.xml. These values are persisted
 * to logs. Entries should not be renumbered, removed or reused.
 *
 * MAX_VALUE should always be at the end to help get the current number of
 * buckets.
 */
export enum CalendarAction {
  EXPANDED_EVENT_HEADER_CLICKED = 0,
  DOUBLE_BOOKED_EVENT_HEADER_CLICKED = 1,
  BASIC_EVENT_HEADER_CLICKED = 2,
  ATTACHMENT_CLICKED = 3,
  CONFERENCE_CALL_CLICKED = 4,
  SEE_MORE_CLICKED = 5,
  MAX_VALUE = SEE_MORE_CLICKED,
}

export function recordCalendarAction(
    action: CalendarAction, moduleName: string) {
  chrome.metricsPrivate.recordEnumerationValue(
      `NewTabPage.${moduleName}.UserAction`, action,
      CalendarAction.MAX_VALUE + 1);
}

export function toJsTimestamp(time: Time): number {
  return Number((time.internalValue - kWindowsToUnixEpochOffset) / 1000n);
}
