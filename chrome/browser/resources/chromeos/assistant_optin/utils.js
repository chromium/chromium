// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

/* Script used to strip anchor links from webview */
export const webviewStripLinksContentScript = {
  name: 'stripLinks',
  matches: ['<all_urls>'],
  js: {
    code: 'document.querySelectorAll(\'a\').forEach(' +
        'function(anchor){anchor.href=\'javascript:void(0)\';})',
  },
  run_at: 'document_end',
};

/**
 * Sanitizer which filters the html snippet with a set of whitelisted tags.
 */
export class HtmlSanitizer {
  constructor() {
    // initialize set of whitelisted tags.
    this.allowedTags = new Set(['b', 'i', 'br', 'p', 'a', 'ul', 'li', 'div']);
  }

  /**
   * Sanitize the html snippet.
   * Only allow the tags in allowedTags.
   *
   * @param {string} content the html snippet to be sanitized.
   * @return {string} sanitized html snippet.
   *
   * @public
   */
  sanitizeHtml(content) {
    const doc = document.implementation.createHTMLDocument();
    const div = doc.createElement('div');
    div.innerHTML = sanitizeInnerHtml(content, {tags: ['i', 'ul', 'li']});
    return sanitizeInnerHtml(
        this.sanitizeNode_(doc, div).innerHTML, {tags: ['i', 'ul', 'li']});
  }

  /**
   * Sanitize the html node.
   *
   * @param {Document} doc document object for sanitize use.
   * @param {Element} node the DOM element to be sanitized.
   * @return {Element} sanitized DOM element.
   *
   * @private
   */
  sanitizeNode_(doc, node) {
    const name = node.nodeName.toLowerCase();
    if (name === '#text') {
      return node;
    }
    if (!this.allowedTags.has(name)) {
      return doc.createTextNode('');
    }

    const copy = doc.createElement(name);
    // Only allow 'href' attribute for tag 'a'.
    if (name === 'a' && node.attributes.length === 1 &&
        node.attributes.item(0).name === 'href') {
      copy.setAttribute('href', node.getAttribute('href'));
    }

    while (node.childNodes.length > 0) {
      const child = node.removeChild(node.childNodes[0]);
      copy.appendChild(this.sanitizeNode_(doc, child));
    }
    return copy;
  }
}

/**
 * Possible native assistant icons
 * Must be in sync with the corresponding c++ enum
 * @enum {number}
 */
export const AssistantNativeIconType = {
  NONE: 0,

  // Web & App Activity.
  WAA: 1,

  // Device Applications Information.
  DA: 2,

  INFO: 3,
};
