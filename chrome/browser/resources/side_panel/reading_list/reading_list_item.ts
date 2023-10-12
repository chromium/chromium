// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './icons.html.js';

import {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {MouseHoverableMixin} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterEntry} from './reading_list.mojom-webui.js';
import {ReadingListApiProxy, ReadingListApiProxyImpl} from './reading_list_api_proxy.js';
import {getTemplate} from './reading_list_item.html.js';

export const MARKED_AS_READ_UI_EVENT = 'reading-list-marked-as-read';

const navigationKeys: Set<string> =
    new Set([' ', 'Enter', 'ArrowRight', 'ArrowLeft']);

export interface ReadingListItemElement {
  $: {
    crUrlListItem: CrUrlListItemElement,
    updateStatusButton: HTMLElement,
    deleteButton: HTMLElement,
  };
}

const ReadingListItemElementBase = MouseHoverableMixin(PolymerElement);

export class ReadingListItemElement extends ReadingListItemElementBase {
  static get is() {
    return 'reading-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      data: Object,
      buttonRipples: Boolean,
      title: {
        computed: 'computeTitle_(data.title)',
        reflectToAttribute: true,
      },
    };
  }

  data: ReadLaterEntry;
  buttonRipples: boolean;
  private apiProxy_: ReadingListApiProxy =
      ReadingListApiProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('auxclick', this.onAuxClick_.bind(this));
    this.addEventListener('contextmenu', this.onContextMenu_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  private computeTitle_(): string {
    return this.data.title;
  }

  override focus() {
    this.$.crUrlListItem.focus();
  }

  private onAuxClick_(e: MouseEvent) {
    if (e.button !== 1) {
      // Not a middle click.
      return;
    }

    this.apiProxy_.openUrl(this.data.url, true, {
      middleButton: true,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }

  private onClick_(e: MouseEvent|KeyboardEvent) {
    this.apiProxy_.openUrl(this.data.url, true, {
      middleButton: false,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }

  private onContextMenu_(e: MouseEvent) {
    this.apiProxy_.showContextMenuForUrl(this.data.url, e.clientX, e.clientY);
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }

    const focusableElements: HTMLElement[] = [
      this.$.crUrlListItem,
      this.$.updateStatusButton,
      this.$.deleteButton,
    ];
    const focusedIndex = focusableElements.indexOf(
        this.shadowRoot!.activeElement as HTMLElement);

    switch (e.key) {
      case ' ':
      case 'Enter':
        this.onClick_(e);
        break;
      case 'ArrowRight':
        if (focusedIndex >= focusableElements.length - 1) {
          focusableElements[0].focus();
        } else {
          focusableElements[focusedIndex + 1].focus();
        }
        break;
      case 'ArrowLeft':
        if (focusedIndex <= 0) {
          focusableElements[focusableElements.length - 1].focus();
        } else {
          focusableElements[focusedIndex - 1].focus();
        }
        break;
      default:
        assertNotReached();
    }
    e.preventDefault();
    e.stopPropagation();
  }

  private onUpdateStatusClick_(e: Event) {
    e.stopPropagation();
    this.apiProxy_.updateReadStatus(this.data.url, !this.data.read);
    if (!this.data.read) {
      this.dispatchEvent(new CustomEvent(
          MARKED_AS_READ_UI_EVENT, {bubbles: true, composed: true}));
    }
  }

  private onItemDeleteClick_(e: Event) {
    e.stopPropagation();
    this.apiProxy_.removeEntry(this.data.url);
  }

  /**
   * @return The appropriate icon for the current state
   */
  private getUpdateStatusButtonIcon_(
      markAsUnreadIcon: string, markAsReadIcon: string): string {
    return this.data.read ? markAsUnreadIcon : markAsReadIcon;
  }

  /**
   * @return The appropriate tooltip for the current state
   */
  private getUpdateStatusButtonTooltip_(
      markAsUnreadTooltip: string, markAsReadTooltip: string): string {
    return this.data.read ? markAsUnreadTooltip : markAsReadTooltip;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reading-list-item': ReadingListItemElement;
  }
  interface HTMLElementEventMap {
    [MARKED_AS_READ_UI_EVENT]: CustomEvent;
  }
}

customElements.define(ReadingListItemElement.is, ReadingListItemElement);
