// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {highlight as addHighlight, removeHighlights} from 'chrome://resources/js/search_highlight_utils.js';
import type {Range} from 'chrome://resources/js/search_highlight_utils.js';

/**
 * Recursively finds and removes all highlight wrappers starting from the
 * shadow root of the given element.
 */
export function unhighlight(element: HTMLElement) {
  if (!element.shadowRoot) {
    return;
  }

  const recursiveWork = (root: Node) => {
    const wrappers = Array.from(
        (root as Element)
            .querySelectorAll<HTMLElement>('.search-highlight-wrapper'));
    if (wrappers.length > 0) {
      removeHighlights(wrappers);
    }

    const children = (root as Element).querySelectorAll?.('*');
    children?.forEach(child => {
      if (child.shadowRoot) {
        recursiveWork(child.shadowRoot);
      }
    });
  };

  recursiveWork(element.shadowRoot);
}

/**
 * Recursively finds text nodes within the given element's shadow root that
 * match the query and wraps matches in a highlight component.
 */
export function highlight(element: HTMLElement, query: string) {
  if (!element.shadowRoot || !query) {
    return;
  }

  // Escapes special regex characters in the query to ensure a literal string
  // match.
  const escapeRegex = (s: string): string => {
    return s.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
  };
  const regex = new RegExp(`(${escapeRegex(query)})`, 'gi');

  const recursiveWork = (root: Node, searchRegex: RegExp) => {
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    const nodesToProcess: Text[] = [];
    while (walker.nextNode()) {
      const node = walker.currentNode;
      if (node.parentElement?.tagName !== 'SCRIPT' &&
          node.parentElement?.tagName !== 'STYLE' &&
          node.textContent?.match(searchRegex)) {
        nodesToProcess.push(node as Text);
      }
    }

    nodesToProcess.forEach(node => {
      const text = node.textContent;
      const ranges: Range[] = [];
      let match;
      searchRegex.lastIndex = 0;
      while ((match = searchRegex.exec(text)) !== null) {
        if (match[1]) {
          ranges.push({start: match.index, length: match[1].length});
        }
      }

      if (ranges.length > 0) {
        addHighlight(node, ranges);
      }
    });

    const children = (root as Element).querySelectorAll?.('*');
    children?.forEach(child => {
      if (child.shadowRoot) {
        recursiveWork(child.shadowRoot, searchRegex);
      }
    });
  };

  recursiveWork(element.shadowRoot, regex);
}
