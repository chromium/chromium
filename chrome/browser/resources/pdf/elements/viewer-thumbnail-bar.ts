// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './viewer-thumbnail.js';

import {assert} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PluginController, PluginControllerEventType} from '../controller.js';

import {getTemplate} from './viewer-thumbnail-bar.html.js';
import {ViewerThumbnailElement} from './viewer-thumbnail.js';

export interface ViewerThumbnailBarElement {
  $: {
    thumbnails: HTMLElement,
  };
}

export class ViewerThumbnailBarElement extends PolymerElement {
  static get is() {
    return 'viewer-thumbnail-bar';
  }

  static get template() {
    return getTemplate();
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

      pageNumbers_: {
        type: Array,
        computed: 'computePageNumbers_(docLength)',
      },
    };
  }

  activePage: number;
  clockwiseRotations: number;
  docLength: number;
  private isPluginActive_: boolean;
  private pageNumbers_: number[];
  private intersectionObserver_: IntersectionObserver;
  private pluginController_: PluginController = PluginController.getInstance();
  private tracker_: EventTracker = new EventTracker();

  // TODO(dhoss): Remove `this.inTest` when implemented a mock plugin
  // controller.
  inTest: boolean = false;

  constructor() {
    super();

    this.isPluginActive_ = this.pluginController_.isActive;

    // Listen to whether the plugin is active. Thumbnails should be hidden
    // when the plugin is inactive.
    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.IS_ACTIVE_CHANGED,
        (e: CustomEvent<boolean>) => this.isPluginActive_ = e.detail);
  }

  override ready() {
    super.ready();

    this.addEventListener('focus', this.onFocus_);
    this.addEventListener('keydown', this.onKeydown_);

    const thumbnailsDiv = this.$.thumbnails;

    this.intersectionObserver_ =
        new IntersectionObserver((entries: IntersectionObserverEntry[]) => {
          entries.forEach(entry => {
            const thumbnail = entry.target as ViewerThumbnailElement;

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
          // The root margin is set to 100% on the bottom to prepare thumbnails
          // that are one standard scroll finger swipe away. The root margin is
          // set to 500% on the top to discard thumbnails that are far from
          // view, but to avoid regenerating thumbnails that are close.
          rootMargin: '500% 0% 100%',
        });

    FocusOutlineManager.forDocument(document);
  }

  /**
   * Changes the focus to the thumbnail of the new active page if the focus was
   * already on a thumbnail.
   */
  private activePageChanged_() {
    if (this.shadowRoot!.activeElement) {
      this.getThumbnailForPage(this.activePage)!.focusAndScroll();
    }
  }

  private clickThumbnailForPage(pageNumber: number) {
    const thumbnail = this.getThumbnailForPage(pageNumber);
    if (!thumbnail) {
      return;
    }

    thumbnail.getClickTarget().click();
  }

  getThumbnailForPage(pageNumber: number): ViewerThumbnailElement|null {
    return this.shadowRoot!.querySelector(
        `viewer-thumbnail:nth-child(${pageNumber})`);
  }

  /** @return The array of page numbers. */
  private computePageNumbers_(): number[] {
    return Array.from({length: this.docLength}, (_, i) => i + 1);
  }

  private getAriaLabel_(pageNumber: number): string {
    return loadTimeData.getStringF('thumbnailPageAriaLabel', pageNumber);
  }

  /** @return Whether the page is the current page. */
  private isActivePage_(page: number): boolean {
    return this.activePage === page;
  }

  private onDomChange_() {
    this.shadowRoot!.querySelectorAll('viewer-thumbnail').forEach(thumbnail => {
      this.intersectionObserver_.observe(thumbnail);
    });
  }

  /** Forwards focus to a thumbnail when tabbing. */
  private onFocus_() {
    // Ignore focus triggered by mouse to allow the focus to go straight to the
    // thumbnail being clicked.
    const focusOutlineManager = FocusOutlineManager.forDocument(document);
    if (!focusOutlineManager.visible) {
      return;
    }

    // Change focus to the thumbnail of the active page.
    const activeThumbnail =
        this.shadowRoot!.querySelector<ViewerThumbnailElement>(
            'viewer-thumbnail[is-active]');
    if (activeThumbnail) {
      activeThumbnail.focus();
      return;
    }

    // Otherwise change to the first thumbnail, if there is one.
    const firstThumbnail = this.shadowRoot!.querySelector('viewer-thumbnail');
    if (!firstThumbnail) {
      return;
    }
    firstThumbnail.focus();
  }

  private onKeydown_(e: KeyboardEvent) {
    switch (e.key) {
      case 'Tab':
        // On shift+tab, first redirect focus from the thumbnails to:
        // 1) Avoid focusing on the thumbnail bar.
        // 2) Focus to the element before the thumbnail bar from any thumbnail.
        if (e.shiftKey) {
          this.focus();
          return;
        }

        // On tab, first redirect focus to the last thumbnail to focus to the
        // element after the thumbnail bar from any thumbnail.
        const lastThumbnail =
            this.shadowRoot!.querySelector<ViewerThumbnailElement>(
                'viewer-thumbnail:last-of-type');
        assert(lastThumbnail);
        lastThumbnail.focus({preventScroll: true});
        break;
      case 'ArrowRight':
      case 'ArrowDown':
        // Prevent default arrow scroll behavior.
        e.preventDefault();
        this.clickThumbnailForPage(this.activePage + 1);
        break;
      case 'ArrowLeft':
      case 'ArrowUp':
        // Prevent default arrow scroll behavior.
        e.preventDefault();
        this.clickThumbnailForPage(this.activePage - 1);
        break;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-thumbnail-bar': ViewerThumbnailBarElement;
  }
}

customElements.define(ViewerThumbnailBarElement.is, ViewerThumbnailBarElement);
