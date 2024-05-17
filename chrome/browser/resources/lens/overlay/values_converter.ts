// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// File to store common utils relating to converting values used across the Lens
// overlay.

// Takes the value between 0-1 and returns a string in the form '__%';
export function toPercent(value: number): string {
  return `${value * 100}%`;
}

// Takes the value and returns a string in the form '__px';
export function toPixels(value: number): string {
  return `${value}px`;
}
