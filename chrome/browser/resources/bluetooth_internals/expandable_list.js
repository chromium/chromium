// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './expandable_list.html.js';

export class ExpandableListElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  connectedCallback() {
    this.classList.add('expandable-list');
  }

  /**
   * Sets the data model of the list.
   * @param {Array} data
   */
  setData(data) {
    this.updateMessageDisplay_(data.length === 0);
    const items = this.shadowRoot.querySelector('.list-items');
    data.forEach(item => {
      const listItem = this.createItem(item);
      items.appendChild(listItem);
    });
  }

  createItem(itemData) {}

  /**
   * Sets the empty message text.
   * @param {string} message
   */
  setEmptyMessage(message) {
    const emptyMessage = this.shadowRoot.querySelector('.empty-message');
    emptyMessage.textContent = message;
  }

  /**
   * Sets the spinner display state. If |showing| is true, the loading
   * spinner is dispayed.
   * @param {boolean} showing
   */
  setSpinnerShowing(showing) {
    this.shadowRoot.querySelector('.spinner').hidden = !showing;
  }

  /**
   * Gets the spinner display state. Returns true if the spinner is showing.
   * @return {boolean}
   */
  isSpinnerShowing() {
    return !this.shadowRoot.querySelector('.spinner').hidden;
  }

  /**
   * Updates the display state of the empty message. If there are no items in
   * the data model, the empty message is displayed.
   * @param {boolean} empty Whether the list is empty.
   */
  updateMessageDisplay_(empty) {
    const emptyMessage = this.shadowRoot.querySelector('.empty-message');
    assert(emptyMessage);
    emptyMessage.hidden = !empty;
  }
}

customElements.define('expandable-list', ExpandableListElement);
