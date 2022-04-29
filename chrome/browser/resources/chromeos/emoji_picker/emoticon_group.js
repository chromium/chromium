// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createCustomEvent, EMOJI_BUTTON_CLICK} from './events.js';
import {CategoryEnum, EmojiVariants} from './types.js';

class EmoticonGroupComponent extends PolymerElement {
  static get is() {
    return 'emoticon-group';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<EmojiVariants>}*/
      data: {type: Array, readonly: true},
    };
  }

  onEmoticonClick(ev) {
    const emoticonString = ev.target.getAttribute('emoticon-string');
    const emoticonName = ev.target.getAttribute('emoticon-name');
    this.dispatchEvent(createCustomEvent(EMOJI_BUTTON_CLICK, {
      text: emoticonString,
      isVariant: false,
      baseEmoji: emoticonString,
      allVariants: [],
      name: emoticonName,
      category: CategoryEnum.EMOTICON
    }));
  }

  /**
   * @param {Number} index
   * @returns {string} unique string to distinguish different emoticons within
   *     same group.
   */
  generateEmoticonId(index) {
    return `emoticon-${index}`;
  }
}

customElements.define(EmoticonGroupComponent.is, EmoticonGroupComponent);
