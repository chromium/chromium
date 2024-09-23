// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'event_utils' contains common functions related to events to reduce repeat
 * code.
 */

// Returns a custom event with provided `eventName` with bubbles and composed
// set to true.
export function createCustomEvent(eventName: string): CustomEvent<void> {
  return new CustomEvent<void>(eventName, {bubbles: true, composed: true});
}
