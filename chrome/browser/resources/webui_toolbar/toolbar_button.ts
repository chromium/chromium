// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function getContextMenuPosition(element: HTMLElement) {
  const bounds = element.getBoundingClientRect();
  const isRtl = document.dir === 'rtl';
  const x = isRtl ? bounds.right : bounds.left;
  return {x, y: bounds.bottom};
}
