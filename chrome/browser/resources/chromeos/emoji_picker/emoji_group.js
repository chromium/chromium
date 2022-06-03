// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './emoji_button.js';

import {beforeNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createCustomEvent, EMOJI_VARIANTS_SHOWN, EMOJI_BUTTON_CLICK, EMOJI_CLEAR_RECENTS_CLICK} from './events.js';
import {CategoryEnum, EmojiVariants} from './types.js';

export class EmojiGroupComponent extends PolymerElement {
  static get is() {
    return 'emoji-group';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<EmojiVariants>} */
      data: {type: Array, readonly: true},
      /** @type {Object<string,string>} */
      preferred: {type: Object, value: {}},
      /** @type {boolean} */
      clearable: {type: Boolean, value: false},
      /** @type {boolean} */
      showClearRecents: {type: Boolean, value: false},
      /** @private {?EmojiVariants} */
      focusedEmoji: {type: Object, value: null},
      /** @type {string} */
      category: {
        type: String,
        value: CategoryEnum.EMOJI,
        readonly: true,
      },
      /** @private {?number} */
      shownEmojiVariantIndex: {type: Number, value: null},
      /** @private {?boolean} */
      isLangEnglish: {type: Boolean, value: false},
      /** @type {boolean} */
      useFlexLayout: {
        type: Boolean,
        value: false,
        readonly: true,
        reflectToAttribute: true,
      },
    };
  }

  constructor() {
    super();

    // TODO(crbug/1227852): Remove after setting arial label to emoji.
    this.isLangEnglish = navigator.languages.some(
      lang => lang.startsWith('en')) > 0;
  }

  /**
   * Handles the click event for show-clear button which results
   * in showing "clear recently used emojis" button.
   *
   * @param {Event} ev
   */
  onClearClick(ev) {
    ev.preventDefault();
    ev.stopPropagation();
    this.showClearRecents = true;
  }

  /**
   * Handles the event for clicking on the "clear recently used" button.
   * It makes "show-clear" button disappear and fires an event
   * indicating that the "clear recently used" is clicked.
   *
   * @fires CustomEvent#`EMOJI_CLEAR_RECENTS_CLICK`
   * @param {Event} ev
   */
  onClearRecentsClick(ev) {
    ev.preventDefault();
    ev.stopPropagation();
    this.showClearRecents = false;
    this.dispatchEvent(createCustomEvent(
      EMOJI_CLEAR_RECENTS_CLICK,  {category: this.category}));
  }

  /**
   * Sets and shows tooltips for an emoji button based on a mouse/focus event.
   * By handling the on-focus and on-mouseenter events using this function,
   * one single paper-tool is reused for all emojis in the emoji-group.
   *
   * @param {MouseEvent|FocusEvent} event
   */
  showTooltip(event) {
    const emoji = this.findEmojiOfEmojiButton(event.target);

    // If the event is for an emoji button that is not already
    // focused, then replace the target of paper-tooltip with the new
    // emoji button.
    if (emoji && this.focusedEmoji !== emoji) {
      this.focusedEmoji = emoji;

      // Set the target of paper-tooltip to the focused emoji button.
      // Paper-tooltip will un-listen the events of the previous target and
      // starts listening the events for the focused emoji button to hide and
      // show the tooltip at the right time.
      this.$.tooltip.target = event.target;
      this.$.tooltip.show();
    }
  }

  /**
   * Handles event of clicking on an emoji button. It finds the emoji details
   * for the clicked emoji and fires another event including these the details.
   * Note: Initially, it validates and returns if the event is not for an
   * emoji button.
   *
   * @fires CustomEvent#`EMOJI_BUTTON_CLICK`
   * @param {MouseEvent} event Click event
   */
  onEmojiClick(event) {
    const emoji = this.findEmojiOfEmojiButton(event.target);

    // Ensure target is an emoji button.
    if (!emoji) {
      return;
    }

    const text = this.getDisplayEmojiForEmoji(emoji.base.string);

    this.dispatchEvent(createCustomEvent(EMOJI_BUTTON_CLICK, {
      text: text,
      isVariant: text !== emoji.base.string,
      baseEmoji: emoji.base.string,
      allVariants: emoji.alternates,
      name: emoji.base.name,
      category: this.category,
    }));
  }

  /**
   * Handles event of opening context menu of an emoji button. Emoji variants
   * are shown as the context menu.
   * Note: Initially, it validates and returns if the event is not for an
   * emoji button.
   *
   * @fires CustomEvent#`EMOJI_VARIANTS_SHOWN`
   * @param {Event} event
   */
  onEmojiContextMenu(event) {
    const emoji = this.findEmojiOfEmojiButton(event.target);

    // Ensure target is an emoji button.
    if (!emoji) {
      return;
    }
    event.preventDefault();

    const dataIndex = parseInt(event.target.getAttribute('data-index'), 10);

    // If the variants of the emoji is already shown, then hide it.
    // Otherwise, show the variants if there are some.
    if (emoji.alternates && emoji.alternates.length &&
        dataIndex !== this.shownEmojiVariantIndex) {
          this.shownEmojiVariantIndex = dataIndex;
    } else {
      this.shownEmojiVariantIndex = null;
    }

    // Send event so emoji-picker knows to close other variants.
    // need to defer this until <emoji-variants> is created and sized by
    // Polymer.
    beforeNextRender(this, () => {
      const variants = this.shownEmojiVariantIndex ?
          this.shadowRoot.getElementById(this.getEmojiVariantId(dataIndex)) :
          null;

      this.dispatchEvent(createCustomEvent(EMOJI_VARIANTS_SHOWN, {
        owner: this,
        variants: variants,
        baseEmoji: emoji.base.string,
      }));
    });
  }

  /**
   * Returns the id of an emoji button element at a specific data index.
   *
   * @param {number} index Index of the emoji button in `this.data`.
   * @returns {string} The id of the element.
   */
  getEmojiButtonId(index) {
    return `emoji-${index}`;
  }

  /**
   * Returns the id of an emoji variants element for an emoji at a
   * specific data index.
   *
   * @param {number} index Index of the emoji in `this.data`.
   * @returns {string} The id of the element.
   */
  getEmojiVariantId(index) {
    return `emoji-variant-${index}`;
  }

  /**
   * Returns HTML class attribute of an emoji button.
   *
   * @param {EmojiVariants} emoji The emoji to be shown.
   * @returns {string} HTML class attribute of the button.
   */
  getEmojiButtonClassName(emoji) {
    if (emoji.alternates && emoji.alternates.length > 0) {
      return 'emoji-button has-variants';
    } else {
      return 'emoji-button';
    }
  }

  /**
   * Returns the arial label of an emoji.
   *
   * @param {EmojiVariants} emoji The emoji to be shown.
   * @returns {string} Arial label for the input emoji.
   */
  getEmojiAriaLabel(emoji) {
    // TODO(crbug/1227852): Just use emoji as the tooltip once ChromeVox can
    // announce them properly.
    const emojiLabel = this.isLangEnglish ?
        emoji.base.name : this.getDisplayEmojiForEmoji(emoji.base.string);
    if (emoji.alternates && emoji.alternates.length > 0) {
      return emojiLabel + ' with variants.';
    } else {
      return emojiLabel;
    }
  }

  /**
   * Returns the character to be shown for the emoji.
   *
   * @param {string} baseEmoji Base emoji character.
   * @returns {string} Character to be shown for the emoji.
   */
  getDisplayEmojiForEmoji(baseEmoji) {
    return this.preferred[baseEmoji] || baseEmoji;
  }

  /**
   * Return weather variants of an emoji is visible or not.
   *
   * @param {number} emojiIndex Index of an emoji in `this.data`.
   * @param {?number} shownEmojiVariantIndex Index of an emoji variant that is
   *    shown.
   * @returns {boolean} True if the variants is shown and false otherwise.
   */
  isEmojiVariantVisible(emojiIndex, shownEmojiVariantIndex) {
    return emojiIndex === shownEmojiVariantIndex;
  }

  /**
   * Hides emoji variants if any is visible.
   */
  hideEmojiVariants() {
    this.shownEmojiVariantIndex = null;
  }

  /**
   * Finds emoji details for an HTML button based on the attribute of
   * data-index and event target information.
   * The result will be null if the target is not for a button element
   * or it does not have data-index attribute.
   *
   * @param {?EventTarget} target focused element
   * @returns {?EmojiVariants} Emoji details.
   */
  findEmojiOfEmojiButton(target){
    // TODO(b/234074956): Use optional chaining when converting to TS.
    const dataIndex = target ? target.getAttribute('data-index') : null;

    if (target.nodeName !== 'BUTTON' || !dataIndex) {
      return null;
    }
    return this.data[parseInt(dataIndex, 10)];
  }

  /**
   * Returns the first emoji button in the group.
   *
   * @returns {HTMLButtonElement} The first button if exist, otherwise null.
   */
   firstEmojiButton() {
    return /** @type {HTMLButtonElement} */ (
      this.shadowRoot.querySelector('.emoji-button'));
  }
}

customElements.define(EmojiGroupComponent.is, EmojiGroupComponent);
