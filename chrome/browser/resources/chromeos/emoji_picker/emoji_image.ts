// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VISUAL_CONTENT_WIDTH} from './constants.js';
import {getTemplate} from './emoji_image.html.js';
import {EmojiVariants} from './types.js';

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
    };
  }

  index: number;
  item: EmojiVariants;
  loading: boolean = true;

  showTooltip: (e: MouseEvent|FocusEvent) => void;
  emojiClick: (e: MouseEvent) => void;

  private handleMouseEnter(event: MouseEvent): void {
    this.showTooltip(event);
  }

  private handleFocus(event: FocusEvent): void {
    this.showTooltip(event);
  }

  private handleClick(event: MouseEvent): void {
    this.emojiClick(event);
  }

  private handleLoad(): void {
    this.loading = false;
  }

  private getImageClassName(loading: boolean) {
    return loading ? 'emoji-image loading' : 'emoji-image';
  }

  /**
   * Returns visual content preview url.
   */
  private getUrl() {
    return this.item.base.visualContent?.url.preview.url;
  }

  private setHeight() {
    const {visualContent} = this.item.base;

    if (visualContent === undefined) {
      return;
    }

    const {height, width} = visualContent.previewSize;
    const visualContentHeight = height / width * VISUAL_CONTENT_WIDTH;
    this.updateStyles({
      '--visual-content-height': `${visualContentHeight}px`,
    });
  }

  override ready() {
    super.ready();
    this.setHeight();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiImageComponent.is]: EmojiImageComponent;
  }
}

customElements.define(EmojiImageComponent.is, EmojiImageComponent);