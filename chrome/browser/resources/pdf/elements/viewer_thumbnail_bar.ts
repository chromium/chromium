// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './viewer_thumbnail.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PluginController, PluginControllerEventType} from '../controller.js';

import type {ViewerThumbnailElement} from './viewer_thumbnail.js';
import {getCss} from './viewer_thumbnail_bar.css.js';
import {getHtml} from './viewer_thumbnail_bar.html.js';

// <if expr="enable_pdf_ink2">
export interface Ink2ThumbnailData {
  type: string;
  pageNumber: number;
  imageData: ArrayBuffer;
  width: number;
  height: number;
}
// </if>

export interface ViewerThumbnailBarElement {
  $: {
    thumbnails: HTMLElement,
  };
}

export class ViewerThumbnailBarElement extends CrLitElement {
  static get is() {
    return 'viewer-thumbnail-bar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      activePage: {type: Number},
      clockwiseRotations: {type: Number},
      docLength: {type: Number},
      isPluginActive_: {type: Boolean},
    };
  }

  activePage: number = 0;
  clockwiseRotations: number = 0;
  docLength: number = 0;
  protected isPluginActive_: boolean = false;
  private intersectionObserver_: IntersectionObserver|null = null;
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

    // <if expr="enable_pdf_ink2">
    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.UPDATE_INK_THUMBNAIL,
        this.handleUpdateInkThumbnail_.bind(this));
    // </if>
  }

  override firstUpdated() {
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

            // Convert to zero-based page index.
            this.pluginController_.requestThumbnail(thumbnail.pageNumber - 1)
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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('activePage')) {
      if (this.shadowRoot!.activeElement) {
        // Changes the focus to the thumbnail of the new active page if the
        // focus was already on a thumbnail.
        this.getThumbnailForPage(this.activePage)!.focusAndScroll();
      }
    }

    if (changedProperties.has('docLength')) {
      assert(this.intersectionObserver_);
      // If doc length changes, we render new thumbnails.
      this.shadowRoot!.querySelectorAll('viewer-thumbnail')
          .forEach(thumbnail => this.intersectionObserver_!.observe(thumbnail));
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
  protected computePageNumbers_(): number[] {
    return Array.from({length: this.docLength}, (_, i) => i + 1);
  }

  protected getAriaLabel_(pageNumber: number): string {
    return loadTimeData.getStringF('thumbnailPageAriaLabel', pageNumber);
  }

  /** @return Whether the page is the current page. */
  protected isActivePage_(page: number): boolean {
    return this.activePage === page;
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

  // <if expr="enable_pdf_ink2">
  private handleUpdateInkThumbnail_(e: CustomEvent<Ink2ThumbnailData>) {
    const data = e.detail;
    const thumbnail = this.getThumbnailForPage(data.pageNumber);
    if (thumbnail) {
      const array = new Uint8ClampedArray(data.imageData);
      thumbnail.ink2Image = new ImageData(array, data.width);
    }
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-thumbnail-bar': ViewerThumbnailBarElement;
  }
}

customElements.define(ViewerThumbnailBarElement.is, ViewerThumbnailBarElement);
