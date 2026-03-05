// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Navigates between items in a list using ArrowUp, ArrowDown, Home, and End.
 * @param e The keyboard event.
 * @param items The list of focusable items.
 */
export function handleKeyboardNavigation(
    e: KeyboardEvent, items: NodeListOf<HTMLElement>|HTMLElement[]) {
  items = Array.from(items);
  const index = items.indexOf(e.target as HTMLElement);
  if (index === -1) {
    return;
  }

  let newIndex = index;
  switch (e.key) {
    case 'ArrowDown':
      newIndex = (index + 1) % items.length;
      break;
    case 'ArrowUp':
      newIndex = (index - 1 + items.length) % items.length;
      break;
    case 'Home':
      newIndex = 0;
      break;
    case 'End':
      newIndex = items.length - 1;
      break;
    default:
      return;
  }

  e.preventDefault();
  items[newIndex]?.focus();
}
