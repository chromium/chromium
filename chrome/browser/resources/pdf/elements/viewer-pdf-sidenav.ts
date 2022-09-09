// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-vars.css.js';
import '../pdf_viewer_shared_style.css.js';
import './icons.html.js';
import './viewer-document-outline.js';
import './viewer-thumbnail-bar.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from '../bookmark_type.js';
import {record, UserAction} from '../metrics.js';

import {getTemplate} from './viewer-pdf-sidenav.html.js';

export interface ViewerPdfSidenavElement {
  $: {
    icons: HTMLElement,
  };
}

export class ViewerPdfSidenavElement extends PolymerElement {
  static get is() {
    return 'viewer-pdf-sidenav';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activePage: Number,

      bookmarks: {
        type: Array,
        value: () => [],
      },

      clockwiseRotations: Number,

      docLength: Number,

      thumbnailView_: {
        type: Boolean,
        value: true,
      },
    };
  }

  activePage: number;
  bookmarks: Bookmark[];
  clockwiseRotations: number;
  docLength: number;
  private thumbnailView_: boolean;

  override ready() {
    super.ready();

    this.$.icons.addEventListener('keydown', this.onKeydown_.bind(this));
  }

  private onThumbnailClick_() {
    record(UserAction.SELECT_SIDENAV_THUMBNAILS);
    this.thumbnailView_ = true;
  }

  private onOutlineClick_() {
    record(UserAction.SELECT_SIDENAV_OUTLINE);
    this.thumbnailView_ = false;
  }

  private outlineButtonClass_(): string {
    return this.thumbnailView_ ? '' : 'selected';
  }

  private thumbnailButtonClass_(): string {
    return this.thumbnailView_ ? 'selected' : '';
  }

  private getAriaSelectedThumbnails_(): string {
    return this.thumbnailView_ ? 'true' : 'false';
  }

  private getAriaSelectedOutline_(): string {
    return this.thumbnailView_ ? 'false' : 'true';
  }

  private getTabIndexThumbnail_(): string {
    return this.thumbnailView_ ? '0' : '-1';
  }

  private getTabIndexOutline_(): string {
    return this.thumbnailView_ ? '-1' : '0';
  }

  private onKeydown_(e: KeyboardEvent) {
    // Up and down arrows should toggle between thumbnail and outline
    // when sidenav is open and an outline exists.
    if ((e.key === 'ArrowUp' || e.key === 'ArrowDown') &&
        this.bookmarks.length > 0) {
      e.preventDefault();
      e.stopPropagation();
      this.thumbnailView_ = !this.thumbnailView_;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-pdf-sidenav': ViewerPdfSidenavElement;
  }
}

customElements.define(ViewerPdfSidenavElement.is, ViewerPdfSidenavElement);
