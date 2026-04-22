// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ACCESSIBILITY_SELECTOR =
    '[aria-live], [role="alert"], [role="status"], [role="log"]';
const UI_KEYWORDS = [
  'advertisement',
  'sponsored',
  'supported by',
  'skip advertisement',
];
const REMOVABLE_TAGS = [
  'DIV',     'SPAN', 'P',   'SECTION', 'ASIDE',      'HEADER', 'FOOTER',
  'ARTICLE', 'MAIN', 'NAV', 'FIGURE',  'FIGCAPTION', 'UL',     'OL',
  'LI',      'B',    'I',   'STRONG',  'EM',
];

function shouldRemoveNode(elt: HTMLElement): boolean {
  // 1. Remove short accessibility announcements.
  if (elt.matches(ACCESSIBILITY_SELECTOR)) {
    if (elt.textContent.trim().length < 200) {
      return true;
    }
  }

  const textContent = elt.textContent;

  // 2. Remove short labels like "Advertisement" or "Sponsored".
  if (textContent.length > 0 && textContent.length < 100) {
    const text = textContent.trim().toLowerCase();
    if (text.length > 0 && text.length < 40 &&
        UI_KEYWORDS.some(keyword => text.includes(keyword))) {
      return true;
    }
  }

  // 3. Remove visually hidden elements.
  if (REMOVABLE_TAGS.includes(elt.tagName)) {
    const rect = elt.getBoundingClientRect();
    if (rect.width === 0 && rect.height === 0 &&
        textContent.trim().length === 0) {
      return true;
    }
  }

  return false;
}

// Removes descendants of the given container that are explicitly UI labels,
// accessibility announcements, or visually hidden wrappers that survive
// distillation. The container itself is never removed.
export function removeExtraneousElementsFrom(container: HTMLElement) {
  const walker = document.createTreeWalker(container, NodeFilter.SHOW_ELEMENT);
  let prevElt: Node = container;
  let elt = walker.nextNode() as HTMLElement | null;

  while (elt) {
    if (shouldRemoveNode(elt)) {
      // Rewind to safety.
      walker.currentNode = prevElt;
      elt.remove();
    } else {
      // Can advance.
      prevElt = elt;
    }
    elt = walker.nextNode() as HTMLElement | null;
  }
}
