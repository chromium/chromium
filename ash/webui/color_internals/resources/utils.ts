// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function getRGBAFromComputedStyle(element: HTMLElement): string {
  const computedStyle = window.getComputedStyle(element);
  return computedStyle.backgroundColor.toString();
}
