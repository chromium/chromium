// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
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
        if (thumbnail.hasAttribute('pending')) {
          return;
        }

        thumbnail.toggleAttribute('pending');
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
}

customElements.define(ViewerThumbnailBarElement.is, ViewerThumbnailBarElement);
