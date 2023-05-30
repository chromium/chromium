// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VISUAL_CONTENT_WIDTH} from './constants.js';
import {getTemplate} from './emoji_image.html.js';
import { createCustomEvent, EMOJI_CLEAR_RECENTS_CLICK } from './events.js';
import {CategoryEnum, EmojiVariants} from './types.js';

export class EmojiImageComponent extends PolymerElement {
  static get is() {
    return 'emoji-image' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      index: Number,
      item: Object,
      showTooltip: Object,
      emojiClick: Object,

      clearable: {type: Boolean, value: false},
      showClearButton: {type: Boolean, value: false},
    };
  }

  index: number;
  item: EmojiVariants;
  loading: boolean = true;

  showTooltip: (e: MouseEvent|FocusEvent) => void;
  emojiClick: (e: MouseEvent) => void;

  clearable: boolean;
  showClearButton: boolean;

  private handleMouseEnter(event: MouseEvent): void {
    this.showTooltip(event);
  }

  private handleFocus(event: FocusEvent): void {
    this.showTooltip(event);
  }

  private handleClick(event: MouseEvent): void {
    this.emojiClick(event);
  }

  private findSiblingEmojiImageByIndex(index: number):
      EmojiImageComponent|null {
    // The shadow root of emoji-group.
    const parentShadowRoot = this.shadowRoot!.host.getRootNode() as ShadowRoot;

    for (const emojiImage of parentShadowRoot.querySelectorAll('emoji-image')) {
      if (emojiImage.index === index) {
        return emojiImage;
      }
    }

    return null;
  }

  private handleKeydown(event: KeyboardEvent): void {
    // The img element where the keyboard event is triggered.
    const target = event.target as HTMLImageElement;

    // Triggers click event to insert the current GIF image.
    if (event.code === 'Enter') {
      event.stopPropagation();
      target.click();
      return;
    }

    // Moves focus to the correct sibling.
    if (event.code === 'Tab') {
      const siblingIndex = this.index + (event.shiftKey ? -1 : +1);
      const sibling = this.findSiblingEmojiImageByIndex(siblingIndex);

      if (sibling !== null) {
        event.preventDefault();
        event.stopPropagation();
        sibling.focus();
        return;
      }
    }
  }

  override focus() {
    this.shadowRoot?.querySelector('img')?.focus();
  }

  private handleLoad(): void {
    this.loading = false;
  }

  private handleContextMenu(evt: Event): void {
    if (this.clearable) {
      evt.preventDefault();
      evt.stopPropagation();
      this.showClearButton = true;
    }
  }

  private handleMouseLeave(): void {
    if (this.showClearButton) {
      this.showClearButton = false;
    }
  }

  private handleClear(evt: Event): void {
    evt.preventDefault();
    evt.stopPropagation();
    this.showClearButton = false;
    this.dispatchEvent(createCustomEvent(
      EMOJI_CLEAR_RECENTS_CLICK, {
        category: CategoryEnum.GIF,
        item: this.item,
      },
    ));
  }

  private getImageClassName(loading: boolean) {
    return loading ? 'emoji-image loading' : 'emoji-image';
  }

  /**
   * Returns visual content preview url.
   */
  private getUrl(item: EmojiVariants) {
    return item.base.visualContent?.url.preview.url;
  }

  private getStyles(item: EmojiVariants) {
    const {visualContent} = item.base;

    if (visualContent === undefined) {
      return;
    }

    const {height, width} = visualContent.previewSize;
    const visualContentHeight = height / width * VISUAL_CONTENT_WIDTH;

    return `--visual-content-height: ${visualContentHeight}px`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiImageComponent.is]: EmojiImageComponent;
  }
}

customElements.define(EmojiImageComponent.is, EmojiImageComponent);