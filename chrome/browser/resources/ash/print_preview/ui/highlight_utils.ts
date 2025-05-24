// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {Range} from 'chrome://resources/js/search_highlight_utils.js';
import {createEmptySearchBubble, highlight, stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';

/**
 * @param element The element to update. Element should have a shadow root.
 * @param query The current search query
 * @param bubbles A map of bubbles created / results found so far.
 * @return The highlight wrappers that were created.
 */
export function updateHighlights(
    element: HTMLElement, query: RegExp|null,
    bubbles: Map<HTMLElement, number>): HTMLElement[] {
  const highlights: HTMLElement[] = [];
  if (!query) {
    return highlights;
  }

  assert(query.global);

  element.shadowRoot!.querySelectorAll('.searchable').forEach(childElement => {
    childElement.childNodes.forEach(node => {
      if (node.nodeType !== Node.TEXT_NODE) {
        return;
      }

      const textContent = node.nodeValue!;
      if (textContent.trim().length === 0) {
        return;
      }

      const strippedText = stripDiacritics(textContent);
      const ranges: Range[] = [];
      for (let match; match = query.exec(strippedText);) {
        ranges.push({start: match.index, length: match[0].length});
      }

      if (ranges.length > 0) {
        // Don't highlight <select> nodes, yellow rectangles can't be
        // displayed within an <option>.
        if (node.parentNode!.nodeName === 'OPTION') {
          // The bubble should be parented by the select node's parent.
          // Note: The bubble's ::after element, a yellow arrow, will not
          // appear correctly in print preview without SPv175 enabled. See
          // https://crbug.com/817058.
          // TODO(crbug.com/40666299): turn on horizontallyCenter when we fix
          // incorrect positioning caused by scrollbar width changing after
          // search finishes.
          assert(node.parentNode);
          assert(node.parentNode.parentNode);
          const bubble = createEmptySearchBubble(
              node.parentNode.parentNode,
              /* horizontallyCenter= */ false);
          const numHits = ranges.length + (bubbles.get(bubble) || 0);
          bubbles.set(bubble, numHits);
          const msgName = numHits === 1 ? 'searchResultBubbleText' :
                                          'searchResultsBubbleText';
          bubble.firstChild!.textContent =
              loadTimeData.getStringF(msgName, numHits);
        } else {
          highlights.push(highlight(node, ranges));
        }
      }
    });
  });

  return highlights;
}
