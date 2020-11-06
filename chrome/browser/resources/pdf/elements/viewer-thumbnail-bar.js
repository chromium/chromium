// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PluginController, PluginControllerEventType} from '../controller.js';
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
      activePage: {
        type: Number,
        observer: 'activePageChanged_',
      },

      clockwiseRotations: Number,

      docLength: Number,

      isPluginActive_: Boolean,

      /** @private {Array<number>} */
      pageNumbers_: {
        type: Array,
        computed: 'computePageNumbers_(docLength)',
      },
    };
  }

  constructor() {
    super();

    // TODO(dhoss): Remove `this.inTest` when implemented a mock plugin
    // controller.
    /** @type {boolean} */
    this.inTest = false;

    /** @private {!PluginController} */
    this.pluginController_ = PluginController.getInstance();

    /** @private {boolean} */
    this.isPluginActive_ = this.pluginController_.isActive;

    /** @private {!EventTracker} */
    this.tracker_ = new EventTracker();

    // Listen to whether the plugin is active. Thumbnails should be hidden
    // when the plugin is inactive.
    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.IS_ACTIVE_CHANGED,
        e => this.isPluginActive_ = e.detail);
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
        const thumbnail = /** @type {!ViewerThumbnailElement} */ (entry.target);

        if (!entry.isIntersecting) {
          thumbnail.clearImage();
          return;
        }

        if (thumbnail.isPainted()) {
          return;
        }
        thumbnail.setPainted();

        if (!this.isPluginActive_ || this.inTest) {
          return;
        }

        this.pluginController_.requestThumbnail(thumbnail.pageNumber)
            .then(response => {
              const array = new Uint8ClampedArray(response.imageData);
              const imageData = new ImageData(array, response.width);
              thumbnail.image = imageData;
            });
      });
    }, {
      root: thumbnailsDiv,
      // The root margin is set to 100% on the bottom to prepare thumbnails that
      // are one standard scroll finger swipe away.
      // The root margin is set to 500% on the top to discard thumbnails that
      // far from view, but to avoid regenerating thumbnails that are close.
      rootMargin: '500% 0% 100%',
    });

    FocusOutlineManager.forDocument(document);
  }

  /**
   * Changes the focus to the thumbnail of the new active page if the focus was
   * already on a thumbnail.
   * @private
   */
  activePageChanged_() {
    if (this.shadowRoot.activeElement) {
      this.getThumbnailForPage(this.activePage).focusAndScroll();
    }
  }

  /**
   * @param {number} pageNumber
   * @private
   */
  clickThumbnailForPage(pageNumber) {
    if (pageNumber < 1 || pageNumber > this.docLength) {
      return;
    }

    this.getThumbnailForPage(pageNumber).getClickTarget().click();
  }

  /**
   * @param {number} pageNumber
   * @return {?ViewerThumbnailElement}
   */
  getThumbnailForPage(pageNumber) {
    return /** @type {ViewerThumbnailElement} */ (this.shadowRoot.querySelector(
        `viewer-thumbnail:nth-child(${pageNumber})`));
  }

  /**
   * @return {!Array<number>} The array of page numbers.
   * @private
   */
  computePageNumbers_() {
    return Array.from({length: this.docLength}, (_, i) => i + 1);
  }

  /**
   * @param {number} pageNumber
   * @return {string}
   * @private
   */
  getAriaLabel_(pageNumber) {
    return loadTimeData.getStringF('thumbnailPageAriaLabel', pageNumber);
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
    } else if (keyboardEvent.key === 'ArrowRight') {
      // Prevent default arrow scroll behavior.
      keyboardEvent.preventDefault();
      this.clickThumbnailForPage(this.activePage + 1);
    } else if (keyboardEvent.key === 'ArrowLeft') {
      // Prevent default arrow scroll behavior.
      keyboardEvent.preventDefault();
      this.clickThumbnailForPage(this.activePage - 1);
    }
  }
}

customElements.define(ViewerThumbnailBarElement.is, ViewerThumbnailBarElement);
