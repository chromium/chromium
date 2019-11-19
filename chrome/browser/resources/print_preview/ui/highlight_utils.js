// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {highlight, highlightControlWithBubble} from 'chrome://resources/js/search_highlight_utils.m.js';

/**
 * @typedef {{
 *   highlights: !Array<!Node>,
 *   bubbles: !Array<!Node>
 * }}
 */
export let HighlightResults;

/**
 * @param {!HTMLElement} element The element to update. Element should have a
 *     shadow root.
 * @param {?RegExp} query The current search query
 * @return {!HighlightResults} The highlight wrappers and
 *     search bubbles that were created.
 */
export function updateHighlights(element, query) {
  const result = {highlights: [], bubbles: []};
  if (!query) {
    return result;
  }

  element.shadowRoot.querySelectorAll('.searchable').forEach(childElement => {
    childElement.childNodes.forEach(node => {
      if (node.nodeType != Node.TEXT_NODE) {
        return;
      }

      const textContent = node.nodeValue.trim();
      if (textContent.length == 0) {
        return;
      }

      if (query.test(textContent)) {
        // Don't highlight <select> nodes, yellow rectangles can't be
        // displayed within an <option>.
        if (node.parentNode.nodeName != 'OPTION') {
          result.highlights.push(highlight(node, textContent.split(query)));
        } else {
          const selectNode = node.parentNode.parentNode;
          // The bubble should be parented by the select node's parent.
          // Note: The bubble's ::after element, a yellow arrow, will not
          // appear correctly in print preview without SPv175 enabled. See
          // https://crbug.com/817058.
          const bubble = highlightControlWithBubble(
              /** @type {!HTMLElement} */ (assert(selectNode.parentNode)),
              textContent.match(query)[0]);
          if (bubble) {
            result.bubbles.push(bubble);
          }
        }
      }
    });
  });
  return result;
}
