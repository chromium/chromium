// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bookmark_folder.html.js';
import {ActionSource} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';

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
    return getTemplate();
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
        computed:
            'computeIsOpen_(openFolders, folder.id, folder.children.length)',
      },

      openFolders: Array,
    };
  }

  private childDepth_: number;
  depth: number;
  folder: chrome.bookmarks.BookmarkTreeNode;
  private open_: boolean;
  openFolders: string[];
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();

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

  private onBookmarkAuxClick_(
      event: DomRepeatEvent<chrome.bookmarks.BookmarkTreeNode, MouseEvent>) {
    if (event.button !== 1) {
      // Not a middle click.
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.openBookmark(
        event.model.item.id!, this.depth, {
          middleButton: true,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        },
        ActionSource.kBookmark);
  }

  private onBookmarkClick_(
      event: DomRepeatEvent<chrome.bookmarks.BookmarkTreeNode, MouseEvent>) {
    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.openBookmark(
        event.model.item.id!, this.depth, {
          middleButton: false,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        },
        ActionSource.kBookmark);
  }

  private onBookmarkContextMenu_(
      event: DomRepeatEvent<chrome.bookmarks.BookmarkTreeNode, MouseEvent>) {
    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.showContextMenu(
        event.model.item.id, event.clientX, event.clientY,
        ActionSource.kBookmark);
  }

  private onFolderContextMenu_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.showContextMenu(
        this.folder.id, event.clientX, event.clientY, ActionSource.kBookmark);
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

    this.dispatchEvent(new CustomEvent(FOLDER_OPEN_CHANGED_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        id: this.folder.id,
        open: !this.open_,
      },
    }));

    chrome.metricsPrivate.recordUserAction(
        this.open_ ? 'SidePanel.Bookmarks.FolderOpen' :
                     'SidePanel.Bookmarks.FolderClose');
  }

  private computeIsOpen_() {
    return Boolean(this.openFolders) &&
        this.openFolders.includes(this.folder.id) && this.folder.children &&
        this.folder.children.length > 0;
  }

  private getFocusableRows_(): HTMLElement[] {
    return Array.from(
        this.shadowRoot!.querySelectorAll('.row, bookmark-folder'));
  }

  getFocusableElement(path: chrome.bookmarks.BookmarkTreeNode[]): (HTMLElement|
                                                                   null) {
    const currentNode = path.shift();
    if (currentNode) {
      const currentNodeId = currentNode.id;
      const currentNodeElement =
          this.shadowRoot!.querySelector(`#bookmark-${currentNodeId}`) as (
              HTMLElement | null);
      if (currentNodeElement &&
          currentNodeElement.classList.contains('bookmark')) {
        // Found a bookmark item.
        return currentNodeElement;
      }

      if (currentNodeElement &&
          currentNodeElement instanceof BookmarkFolderElement) {
        // Bookmark item may be a grandchild or be deeper. Iterate through
        // child BookmarkFolderElements until the bookmark item is found.
        const nestedElement = currentNodeElement.getFocusableElement(path);
        if (nestedElement) {
          return nestedElement;
        }
      }
    }

    // If all else fails, return the focusable folder row.
    return this.shadowRoot!.querySelector('#folder');
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

declare global {
  interface HTMLElementTagNameMap {
    'bookmark-folder': BookmarkFolderElement;
  }
}

customElements.define(BookmarkFolderElement.is, BookmarkFolderElement);

interface DraggableElement extends HTMLElement {
  dataBookmark: chrome.bookmarks.BookmarkTreeNode;
}

export function getBookmarkFromElement(element: HTMLElement):
    chrome.bookmarks.BookmarkTreeNode {
  return (element as DraggableElement).dataBookmark;
}

export function isValidDropTarget(element: HTMLElement) {
  return element.id === 'folder' || element.classList.contains('bookmark');
}

export function isBookmarkFolderElement(element: HTMLElement): boolean {
  return element.id === 'folder';
}
