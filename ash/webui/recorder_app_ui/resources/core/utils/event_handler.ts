// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An event handler that preventDefault and stopPropagation on the event.
 */
export function suppressEvent(event: Event): void {
  event.stopPropagation();
  event.preventDefault();
}

/**
 * An event handler that stopPropagation on the event.
 */
export function stopPropagation(event: Event): void {
  event.stopPropagation();
}
