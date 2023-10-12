// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

function getRequiredElement(id) {
  // Disable getElementById restriction here, because this UI uses non valid
  // selectors that don't work with querySelector().
  // eslint-disable-next-line no-restricted-properties
  const el = document.getElementById(id);
  assert(el instanceof HTMLElement);
  return el;
}

/**
 * Finds a good place to set initial focus. Generally called when UI is shown.
 * @param {!Element} root Where to start looking for focusable controls.
 */
function setInitialFocus(root) {
  // Do not change focus if any element in |root| is already focused.
  if (root.contains(document.activeElement)) {
    return;
  }

  const elements =
      root.querySelectorAll('input, list, select, textarea, button');
  for (let i = 0; i < elements.length; i++) {
    const element = elements[i];
    element.focus();
    // .focus() isn't guaranteed to work. Continue until it does.
    if (document.activeElement === element) {
      return;
    }
  }
}

/**
 * Base class for pages that can be shown and hidden by PageManager. Each Page
 * is like a node in a forest, corresponding to a particular div. At any
 * point, one root Page is visible, and any visible Page can show a child Page
 * as an overlay. The host of the root Page(s) should provide a container div
 * for each nested level to enforce the stack order of overlays.
 */
export class Page extends EventTarget {
  /**
   * @param {string} name Page name.
   * @param {string} title Page title, used for history.
   * @param {string} pageDivName ID of the div corresponding to the page.
   */
  constructor(name, title, pageDivName) {
    super();

    this.name = name;
    this.title = title;
    this.pageDivName = pageDivName;
    this.pageDiv = getRequiredElement(this.pageDivName);
    // |pageDiv.page| is set to the page object (this) when the page is
    // visible to track which page is being shown when multiple pages can
    // share the same underlying div.
    this.pageDiv.page = null;
    this.tab = null;
    this.lastFocusedElement = null;
    this.hash = '';

    /**
     * The parent page of this page; or null for root pages.
     * @type {Page}
     */
    this.parentPage = null;

    /**
     * The section on the parent page that is associated with this page.
     * Can be null.
     * @type {Element}
     */
    this.associatedSection = null;

    /**
     * An array of controls that are associated with this page. The first
     * control should be located on a root page.
     * @type {Array<Element>}
     */
    this.associatedControls = null;
  }

  /**
   * Initializes page content.
   */
  initializePage() {}

  /**
   * Called by the PageManager when this.hash changes while the page is
   * already visible. This is analogous to the hashchange DOM event.
   */
  didChangeHash() {}

  /**
   * Sets focus on the first focusable element. Override for a custom focus
   * strategy.
   */
  focus() {
    setInitialFocus(this.pageDiv);
  }

  /**
   * Updates the hash of the current page. If the page is topmost, the history
   * state is updated.
   * @param {string} hash The new hash value. Like location.hash, this
   *     should include the leading '#' if not empty.
   */
  setHash(hash) {
    if (this.hash === hash) {
      return;
    }
    this.hash = hash;
    this.dispatchEvent(new CustomEvent('page-hash-changed'));
  }

  /**
   * Called after the page has been shown.
   */
  didShowPage() {}

  /**
   * Called before the page will be hidden, e.g., when a different root page
   * will be shown.
   */
  willHidePage() {}

  /**
   * Called after the overlay has been closed.
   */
  didClosePage() {}

  /**
   * Gets page visibility state.
   * @type {boolean}
   */
  get visible() {
    if (this.pageDiv.hidden) {
      return false;
    }
    return this.pageDiv.page === this;
  }

  /**
   * Sets page visibility.
   * @type {boolean}
   */
  set visible(visible) {
    if ((this.visible && visible) || (!this.visible && !visible)) {
      return;
    }

    this.pageDiv.page = this;
    this.pageDiv.hidden = !visible;
  }
}
