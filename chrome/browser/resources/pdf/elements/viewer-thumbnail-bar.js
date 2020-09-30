// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ViewerThumbnailElement} from './viewer-thumbnail.js';

export class ViewerThumbnailBarElement extends PolymerElement {
  static get is() {
    return 'viewer-thumbnail-bar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      activePage: Number,

      clockwiseRotations: Number,

      docLength: Number,

      /** @private {Array<number>} */
      pageNumbers_: {
        type: Array,
        computed: 'computePageNumbers_(docLength)',
      },
    };
  }

  ready() {
    super.ready();

    this.addEventListener('focus', this.onFocus_);
    this.addEventListener('keydown', this.onKeydown_);

    const thumbnailsDiv = this.shadowRoot.querySelector('#thumbnails');
    assert(thumbnailsDiv);

    /** @private {!IntersectionObserver} */
    this.intersectionObserver_ = new IntersectionObserver(entries => {
      entries.forEach(entry => {
        if (!entry.isIntersecting) {
          // TODO(crbug.com/652400): Unpaint thumbnails.
          return;
        }

        const thumbnail = /** @type {!ViewerThumbnailElement} */ (entry.target);
        if (thumbnail.isPending()) {
          return;
        }

        thumbnail.setPending();
        this.dispatchEvent(new CustomEvent(
            'paint-thumbnail',
            {detail: thumbnail, bubbles: true, composed: true}));
      });
    }, {
      root: thumbnailsDiv,
      // The vertical root margin is set to 100% to also track thumbnails that
      // are one standard finger swipe away.
      rootMargin: '100% 0%',
    });

    FocusOutlineManager.forDocument(document);
  }

  /**
   * @return {!Array<number>} The array of page numbers.
   * @private
   */
  computePageNumbers_() {
    return Array.from({length: this.docLength}, (_, i) => i + 1);
  }

  /**
   * @param {number} page
   * @return {boolean} Whether the page is the current page.
   * @private
   */
  isActivePage_(page) {
    return this.activePage === page;
  }

  /** @private */
  onDomChange_() {
    this.shadowRoot.querySelectorAll('viewer-thumbnail').forEach(thumbnail => {
      this.intersectionObserver_.observe(thumbnail);
    });
  }

  /**
   * Forwards focus to a thumbnail when tabbing.
   * @private
   */
  onFocus_() {
    // Ignore focus triggered by mouse to allow the focus to go straight to the
    // thumbnail being clicked.
    const focusOutlineManager = FocusOutlineManager.forDocument(document);
    if (!focusOutlineManager.visible) {
      return;
    }

    // Change focus to the thumbnail of the active page.
    const activeThumbnail =
        this.shadowRoot.querySelector('viewer-thumbnail[is-active]');
    if (activeThumbnail) {
      activeThumbnail.focus();
      return;
    }

    // Otherwise change to the first thumbnail, if there is one.
    const firstThumbnail = this.shadowRoot.querySelector('viewer-thumbnail');
    if (!firstThumbnail) {
      return;
    }
    firstThumbnail.focus();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeydown_(e) {
    const keyboardEvent = /** @type {!KeyboardEvent} */ (e);
    if (keyboardEvent.key === 'Tab') {
      // On shift+tab, first redirect focus from the thumbnails to:
      // 1) Avoid focusing on the thumbnail bar.
      // 2) Focus to the element before the thumbnail bar from any thumbnail.
      if (e.shiftKey) {
        this.focus();
        return;
      }

      // On tab, first redirect focus to the last thumbnail to focus to the
      // element after the thumbnail bar from any thumbnail.
      this.shadowRoot.querySelector('viewer-thumbnail:last-of-type').focus({
        preventScroll: true
      });
    }
  }
}

customElements.define(ViewerThumbnailBarElement.is, ViewerThumbnailBarElement);
