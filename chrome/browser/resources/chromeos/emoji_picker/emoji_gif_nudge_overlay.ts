// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_gif_nudge_overlay.html.js';
import {EmojiPickerApiProxy} from './emoji_picker_api_proxy.js';



export class EmojiGifNudgeOverlay extends PolymerElement {
  static get is() {
    return 'emoji-gif-nudge-overlay' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      closeOverlay: Object,
    };
  }

  closeOverlay: () => void;

  onClickLink() {
    EmojiPickerApiProxy.getInstance().openHelpCentreArticle();
  }

  onClickTooltip(event: MouseEvent) {
    // If user clicks the tooltip (rather than the background of overlay),
    // we should not close the overlay.
    event.stopPropagation();
  }

  onClickOverlay() {
    this.closeOverlay();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiGifNudgeOverlay.is]: EmojiGifNudgeOverlay;
  }
}

customElements.define(EmojiGifNudgeOverlay.is, EmojiGifNudgeOverlay);
