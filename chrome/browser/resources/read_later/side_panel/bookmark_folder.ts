// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from '../read_later_api_proxy.js';

import {BookmarksApiProxy} from './bookmarks_api_proxy.js';

/** Event interface for dom-repeat. */
interface RepeaterMouseEvent extends MouseEvent {
  clientX: number;
  clientY: number;
  model: {
    item: chrome.bookmarks.BookmarkTreeNode,
  };
}

export interface BookmarkFolderElement {
  $: {
    children: HTMLElement,
  };
}

// Event name for open state of a folder being changed.
export const FOLDER_OPEN_CHANGED_EVENT = 'bookmark-folder-open-changed';

export class BookmarkFolderElement extends PolymerElement {
  static get is() {
    return 'bookmark-folder';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      childDepth_: {
        type: Number,
        value: 1,
      },

      depth: {
        type: Number,
        observer: 'onDepthChanged_',
        value: 0,
      },

      folder: Object,

      open_: {
        type: Boolean,
        value: false,
      },

      openFolders: {
        type: Array,
        observer: 'onOpenFoldersChanged_',
      },
    };
  }

  private childDepth_: number;
  depth: number;
  folder: chrome.bookmarks.BookmarkTreeNode;
  private open_: boolean;
  openFolders: string[];
  private bookmarksApi_: BookmarksApiProxy = BookmarksApiProxy.getInstance();

  static get observers() {
    return [
      'onChildrenLengthChanged_(folder.children.length)',
    ];
  }

  private getAriaExpanded_(): string|undefined {
    if (!this.folder.children || this.folder.children.length === 0) {
      // Remove the attribute for empty folders that cannot be expanded.
      return undefined;
    }

    return this.open_ ? 'true' : 'false';
  }

  private onBookmarkAuxClick_(event: RepeaterMouseEvent) {
    if (event.button !== 1) {
      // Not a middle click.
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.openBookmark(event.model.item.url!, this.depth, {
      middleButton: true,
      altKey: event.altKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
    });
  }

  private onBookmarkClick_(event: RepeaterMouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.openBookmark(event.model.item.url!, this.depth, {
      middleButton: false,
      altKey: event.altKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
    });
  }

  private onBookmarkContextMenu_(event: RepeaterMouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.showContextMenu(
        event.model.item.id, event.clientX, event.clientY);
  }

  private onFolderContextMenu_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.showContextMenu(
        this.folder.id, event.clientX, event.clientY);
  }

  private getBookmarkIcon_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  private onChildrenLengthChanged_() {
    if (this.folder.children) {
      this.style.setProperty(
          '--child-count', this.folder.children!.length.toString());
    } else {
      this.style.setProperty('--child-count', '0');
    }
  }

  private onDepthChanged_() {
    this.childDepth_ = this.depth + 1;
    this.style.setProperty('--node-depth', `${this.depth}`);
    this.style.setProperty('--child-depth', `${this.childDepth_}`);
  }

  private onFolderClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();

    if (!this.folder.children || this.folder.children.length === 0) {
      // No reason to open if there are no children to show.
      return;
    }

    this.open_ = !this.open_;
    this.dispatchEvent(new CustomEvent(FOLDER_OPEN_CHANGED_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        id: this.folder.id,
        open: this.open_,
      }
    }));

    chrome.metricsPrivate.recordUserAction(
        this.open_ ? 'SidePanel.Bookmarks.FolderOpen' :
                     'SidePanel.Bookmarks.FolderClose');
  }

  private onOpenFoldersChanged_() {
    this.open_ =
        Boolean(this.openFolders) && this.openFolders.includes(this.folder.id);
  }

  private getFocusableRows_(): HTMLElement[] {
    return Array.from(
        this.shadowRoot!.querySelectorAll('.row, bookmark-folder'));
  }

  moveFocus(delta: -1|1): boolean {
    const currentFocus = this.shadowRoot!.activeElement;
    if (currentFocus instanceof BookmarkFolderElement &&
        currentFocus.moveFocus(delta)) {
      // If focus is already inside a nested folder, delegate the focus to the
      // nested folder and return early if successful.
      return true;
    }

    let moveFocusTo = null;
    const focusableRows = this.getFocusableRows_();
    if (currentFocus) {
      // If focus is in this folder, move focus to the next or previous
      // focusable row.
      const currentFocusIndex =
          focusableRows.indexOf(currentFocus as HTMLElement);
      moveFocusTo = focusableRows[currentFocusIndex + delta];
    } else {
      // If focus is not in this folder yet, move focus to either end.
      moveFocusTo = delta === 1 ? focusableRows[0] :
                                  focusableRows[focusableRows.length - 1];
    }

    if (moveFocusTo instanceof BookmarkFolderElement) {
      return moveFocusTo.moveFocus(delta);
    } else if (moveFocusTo) {
      moveFocusTo.focus();
      return true;
    } else {
      return false;
    }
  }
}

customElements.define(BookmarkFolderElement.is, BookmarkFolderElement);

interface DraggableElement extends HTMLElement {
  dataBookmark: chrome.bookmarks.BookmarkTreeNode;
}

export function getBookmarkFromElement(element: HTMLElement) {
  return (element as DraggableElement).dataBookmark;
}

export function isValidDropTarget(element: HTMLElement) {
  return element.id === 'folder' || element.classList.contains('bookmark');
}

export function isBookmarkFolderElement(element: HTMLElement): boolean {
  return element.id === 'folder';
}