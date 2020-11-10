// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-localized-link' takes a localized string that
 * contains up to one anchor tag, and labels the string contained within the
 * anchor tag with the entire localized string. The string should not be bound
 * by element tags. The string should not contain any elements other than the
 * single anchor tagged element that will be aria-labelledby the entire string.
 *
 * Example: "lorem ipsum <a href="example.com">Learn More</a> dolor sit"
 *
 * The "Learn More" will be aria-labelledby like so: "lorem ipsum Learn More
 * dolor sit". Meanwhile, "Lorem ipsum" and "dolor sit" will be aria-hidden.
 *
 * This element also supports strings that do not contain anchor tags; in this
 * case, the element gracefully falls back to normal text. This can be useful
 * when the property is data-bound to a function which sometimes returns a
 * string with a link and sometimes returns a normal string.
 */

Polymer({
  is: 'settings-localized-link',

  properties: {
    /**
     * The localized string that contains up to one anchor tag, the text
     * within which will be aria-labelledby the entire localizedString.
     */
    localizedString: String,

    /**
     * If provided, the URL that the anchor tag will point to. There is no
     * need to provide a linkUrl if the URL is embedded in the localizedString.
     */
    linkUrl: {
      type: String,
      value: '',
    },
  },

  /**
   * Attaches aria attributes and optionally provided link to the provided
   * localizedString.
   * @param {string} localizedString
   * @param {string} linkUrl
   * @return {string} localizedString formatted with additional ids, spans,
   *     and an aria-labelledby tag
   * @private
   */
  getAriaLabelledContent_(localizedString, linkUrl) {
    const tempEl = document.createElement('div');
    tempEl.innerHTML = localizedString;

    const ariaLabelledByIds = [];
    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType === Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `id${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', true);
        node.replaceWith(spanNode);
        return;
      }
      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A') {
        node.id = `id${index}`;
        ariaLabelledByIds.push(node.id);
        return;
      }

      // Only text and <a> nodes are allowed.
      assertNotReached('settings-localized-link has invalid node types');
    });

    const anchorTags = tempEl.getElementsByTagName('a');
    // In the event the provided localizedString contains only text nodes,
    // populate the contents with the provided localizedString.
    if (anchorTags.length === 0) {
      return localizedString;
    }

    assert(
        anchorTags.length === 1,
        'settings-localized-link should contain exactly one anchor tag');
    const anchorTag = anchorTags[0];
    anchorTag.setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));

    if (linkUrl !== '') {
      anchorTag.href = linkUrl;
      anchorTag.target = '_blank';
    }

    return tempEl.innerHTML;
  },

  /** @override */
  attached() {
    const anchorTag = this.$$('a');
    if (anchorTag) {
      anchorTag.addEventListener('click', this.onAnchorTagClick_.bind(this));
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAnchorTagClick_(event) {
    this.fire('link-clicked', {event});
    // Stop propagation of the event, since it has already been handled by
    // opening the link.
    event.stopPropagation();
  },
});
