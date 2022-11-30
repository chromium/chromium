// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './power_bookmark_chip.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmark_row.html.js';

export interface PowerBookmarkRowElement {
  $: {
    bookmarkImage: HTMLDivElement,
  };
}

export class PowerBookmarkRowElement extends PolymerElement {
  static get is() {
    return 'power-bookmark-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      bookmark: {
        type: Object,
        observer: 'updateImage_',
      },

      compact: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: 'updateImage_',
      },

      description: {
        type: String,
        value: '',
      },
    };
  }

  bookmark: chrome.bookmarks.BookmarkTreeNode;
  compact: boolean;

  /**
   * Add the appropriate image for the given bookmark and compact/expanded
   * state. If the bookmark should be displayed as compact, this image will be
   * a favicon or folder icon, otherwise it will be a larger image.
   */
  private updateImage_() {
    // Reset styling added in previous calls to this method.
    this.$.bookmarkImage.classList.remove('url-icon');
    this.$.bookmarkImage.classList.remove('icon-folder-open');
    this.$.bookmarkImage.style.backgroundImage = '';
    this.$.bookmarkImage.style.backgroundColor = '';
    if (this.compact) {
      if (this.bookmark.url) {
        this.$.bookmarkImage.classList.add('url-icon');
        this.$.bookmarkImage.style.backgroundImage =
            getFaviconForPageURL(this.bookmark.url, false);
      } else {
        this.$.bookmarkImage.classList.add('icon-folder-open');
      }
    } else {
      // TODO(b/244627092): Add image once available
      this.$.bookmarkImage.style.backgroundColor = 'red';
    }
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the row.
   */
  private onRowClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('row-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        event: event,
      },
    }));
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'power-bookmark-row': PowerBookmarkRowElement;
  }
}

customElements.define(PowerBookmarkRowElement.is, PowerBookmarkRowElement);
