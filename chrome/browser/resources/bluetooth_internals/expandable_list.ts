// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './expandable_list.html.js';

export abstract class ExpandableListElement<T> extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  connectedCallback() {
    this.classList.add('expandable-list');
  }

  /**
   * Sets the data model of the list.
   */
  setData(data: T[]) {
    this.updateMessageDisplay_(data.length === 0);
    const items = this.shadowRoot!.querySelector('.list-items');
    assert(items);
    data.forEach(item => {
      const listItem = this.createItem(item);
      items.appendChild(listItem);
    });
  }

  abstract createItem(itemData: T): Element;

  /**
   * Sets the empty message text.
   */
  setEmptyMessage(message: string) {
    const emptyMessage = this.shadowRoot!.querySelector('.empty-message');
    assert(emptyMessage);
    emptyMessage.textContent = message;
  }

  /**
   * Sets the spinner display state. If |showing| is true, the loading
   * spinner is displayed.
   */
  setSpinnerShowing(showing: boolean) {
    const spinner = this.shadowRoot!.querySelector<HTMLElement>('.spinner');
    assert(spinner);
    spinner.hidden = !showing;
  }

  /**
   * Gets the spinner display state. Returns true if the spinner is showing.
   */
  isSpinnerShowing(): boolean {
    const spinner = this.shadowRoot!.querySelector<HTMLElement>('.spinner');
    assert(spinner);
    return !spinner.hidden;
  }

  /**
   * Updates the display state of the empty message. If there are no items in
   * the data model, the empty message is displayed.
   * @param empty Whether the list is empty.
   */
  private updateMessageDisplay_(empty: boolean) {
    const emptyMessage =
        this.shadowRoot!.querySelector<HTMLElement>('.empty-message');
    assert(emptyMessage);
    emptyMessage.hidden = !empty;
  }
}
