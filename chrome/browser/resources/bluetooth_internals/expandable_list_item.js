// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './expandable_list_item.html.js';

/**
 * A list item that has expandable content that toggles when the item is
 * clicked.
 */
export class ExpandableListItemElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  connectedCallback() {
    const briefContent = this.shadowRoot.querySelector('.brief-content');
    briefContent.addEventListener('click', () => this.onExpand_());
    this.classList.add('expandable-list-item');
  }

  /** @private */
  onExpand_() {
    const expanded = !this.hasAttribute('expanded');
    this.toggleAttribute('expanded', expanded);
    this.dispatchEvent(new CustomEvent(
        'list-item-expanded',
        {bubbles: true, composed: true, detail: expanded}));
  }
}

customElements.define('expandable-list-item', ExpandableListItemElement);
