// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './icons.html.js';

import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {MouseHoverableMixinLit} from 'chrome://resources/cr_elements/mouse_hoverable_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ReadLaterEntry} from './reading_list.mojom-webui.js';
import type {ReadingListApiProxy} from './reading_list_api_proxy.js';
import {ReadingListApiProxyImpl} from './reading_list_api_proxy.js';
import {getCss} from './reading_list_item.css.js';
import {getHtml} from './reading_list_item.html.js';

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

const ReadingListItemElementBase = MouseHoverableMixinLit(CrLitElement);

export class ReadingListItemElement extends ReadingListItemElementBase {
  static get is() {
    return 'reading-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
      buttonRipples: {type: Boolean},
    };
  }

  data: ReadLaterEntry = {
    title: '',
    url: {url: ''},
    displayUrl: '',
    updateTime: 0n,
    read: false,
    displayTimeSinceUpdate: '',
  };

  buttonRipples: boolean = false;
  private apiProxy_: ReadingListApiProxy =
      ReadingListApiProxyImpl.getInstance();

  override firstUpdated() {
    this.addEventListener('click', this.onClick_);
    this.addEventListener('auxclick', this.onAuxClick_.bind(this));
    this.addEventListener('contextmenu', this.onContextMenu_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
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
      case 'ArrowRight': {
        const index =
            focusedIndex >= focusableElements.length - 1 ? 0 : focusedIndex + 1;
        focusableElements[index]!.focus();
        break;
      }
      case 'ArrowLeft': {
        const index =
            focusedIndex <= 0 ? focusableElements.length - 1 : focusedIndex - 1;
        focusableElements[index]!.focus();
        break;
      }
      default:
        assertNotReached();
    }
    e.preventDefault();
    e.stopPropagation();
  }

  protected onUpdateStatusClick_(e: Event) {
    e.stopPropagation();
    this.apiProxy_.updateReadStatus(this.data.url, !this.data.read);
    if (!this.data.read) {
      this.dispatchEvent(new CustomEvent(
          MARKED_AS_READ_UI_EVENT, {bubbles: true, composed: true}));
    }
  }

  protected onItemDeleteClick_(e: Event) {
    e.stopPropagation();
    this.apiProxy_.removeEntry(this.data.url);
  }

  /**
   * @return The appropriate icon for the current state
   */
  protected getUpdateStatusButtonIcon_(
      markAsUnreadIcon: string, markAsReadIcon: string): string {
    return this.data.read ? markAsUnreadIcon : markAsReadIcon;
  }

  /**
   * @return The appropriate tooltip for the current state
   */
  protected getUpdateStatusButtonTooltip_(
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
