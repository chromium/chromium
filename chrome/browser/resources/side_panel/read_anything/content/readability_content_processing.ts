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
const INLINE_TAGS = [
  'SPAN',
  'B',
  'I',
  'STRONG',
  'EM',
];
const REMOVABLE_TAGS = [
  'DIV',
  'P',
  'SECTION',
  'ASIDE',
  'HEADER',
  'FOOTER',
  'ARTICLE',
  'MAIN',
  'NAV',
  'FIGURE',
  'FIGCAPTION',
  'UL',
  'OL',
  'LI',
  ...INLINE_TAGS,
];
function hasImage(elt: HTMLElement): boolean {
  return elt.querySelector('img, canvas, picture') !== null;
}

// Some sites use inline whitespace to separate words, which can cause the
// words to be displayed with no whitespace between them when Readability
// filters out empty elements.
function isInlineWhitespaceSeparator(elt: HTMLElement): boolean {
  if (!INLINE_TAGS.includes(elt.tagName) || elt.textContent.length === 0 ||
      elt.textContent.trim().length > 0) {
    return false;
  }

  const prevText = elt.previousSibling?.textContent?.trim() ?? '';
  const nextText = elt.nextSibling?.textContent?.trim() ?? '';
  return prevText.length > 0 && nextText.length > 0;
}

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
      if (hasImage(elt)) {
        return false;
      }
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
      if (isInlineWhitespaceSeparator(elt)) {
        // If an element is an inline whitespace separator, it shouldn't be
        // entirely removed. Instead, it should be replaced with a regular
        // space. This prevents readability from distilling an entire paragraph
        // as a single block of text with no spaces.
        elt.replaceWith(' ');
      } else {
        elt.remove();
      }
    } else {
      // Can advance.
      prevElt = elt;
    }
    elt = walker.nextNode() as HTMLElement | null;
  }
}
