// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './keyboard_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './keyboard_key.html.js';

// TODO(michaelcheco): Add unit test coverage for keyboard-key.

/**
 * @fileoverview
 * 'keyboard-key' provides a visual representation of a single key for the
 * 'keyboard-diagram' component. A single 'keyboard-key' can display one "main
 * glyph" and up to four "corner glyphs".
 *
 * The main glyph can be a text string (specified by 'main-glyph') or an icon
 * (specified by 'icon'). By default it is centered vertically and horizontally
 * on the key, and icons are scaled to fill the key while maintaining their
 * aspect ratio. The main glyph supports ellipsing text strings that don't fit
 * on the key, making it suitable for long labels such as "backspace" as well as
 * letter keys with single characters on them.
 *
 * Adding the 'left' or 'right' classes to the key will align the main glyph to
 * the left or right side respectively, and set icon widths to 24px.
 *
 * The four corner glyphs ('top-left-glyph', 'bottom-left-glyph', etc.) must be
 * text strings, generally single characters. If at least one of the right-side
 * glyphs is set, the corner glyphs will be arranged in the four quadrants of
 * the key like so:
 *
 * +---+
 * |a c|
 * |   |
 * |b d|
 * +---+
 *
 * If neither right-side glyph is set, the "left" glyphs will be centered
 * horizontally, like so:
 *
 * +---+
 * | a |
 * |   |
 * | b |
 * +---+
 *
 * Both a main glyph and one or more corner glyphs may be set, for example for
 * the 'e' key on a German keyboard, which has a centered main glyph but also a
 * Euro symbol in the bottom-right:
 *
 * +---+
 * |   |
 * | e |
 * |  €|
 * +---+
 */

/**
 * Enum of key states.
 * @enum {string}
 */
export const KeyboardKeyState = {
  /** The key has not been pressed during this test session. */
  NOT_PRESSED: 'not-pressed',
  /** The key is currently pressed. */
  PRESSED: 'pressed',
  /** The key is not currently pressed, but we've seen it pressed previously. */
  TESTED: 'tested',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const KeyboardKeyElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class KeyboardKeyElement extends KeyboardKeyElementBase {
  static get is() {
    return 'keyboard-key';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The text to show on the key, if any.
       * @type {?string}
       */
      mainGlyph: String,

      /**
       * The text to show in the top-left of the key (or top-center if no
       * right-side glyphs are set).
       * @type {?string}
       */
      topLeftGlyph: String,

      /**
       * The text to show in the top-right of the key.
       * @type {?string}
       */
      topRightGlyph: String,

      /**
       * The text to show in the bottom-left of the key (or bottom-center if no
       * right-side glyphs are set).
       * @type {?string}
       */
      bottomLeftGlyph: String,

      /**
       * The text to show in the bottom-right of the key.
       * @type {?string}
       */
      bottomRightGlyph: String,

      /** @protected {boolean} */
      showCornerGlyphs_: {
        type: Boolean,
        computed: 'computeShowCornerGlyphs_(' +
            'topLeftGlyph, topRightGlyph, bottomLeftGlyph, bottomRightGlyph)',
      },

      /** @protected {boolean} */
      showSecondColumn_: {
        type: Boolean,
        computed: 'computeShowSecondColumn_(topRightGlyph, bottomRightGlyph)',
      },

      /**
       * The name of the icon to use, if any. The name should be of the form:
       * `iconset_name:icon_name`.
       * @type {?string}
       */
      icon: String,

      /**
       * The state to display the key in.
       * @type {!KeyboardKeyState}
       */
      state: {
        type: String,
        value: KeyboardKeyState.NOT_PRESSED,
        reflectToAttribute: true,
        observer: KeyboardKeyElement.prototype.keyboardKeyStateChanged,
      },

      /**
       * The key name to report to assistive technologies. Defaults to mainGlyph
       * if not set.
       * @type {?string}
       */
      ariaName: String,

      /** @protected {string} */
      ariaLabel_: {
        type: String,
        computed: 'computeAriaLabel_(' +
            'ariaName, mainGlyph, bottomLeftGlyph, bottomRightGlyph, state)',
      },
    };
  }

  computeAriaLabel_(
      ariaName, mainGlyph, bottomLeftGlyph, bottomRightGlyph, state) {
    const name =
        ariaName || mainGlyph || bottomRightGlyph || bottomLeftGlyph || '';
    const stateStringIds = {
      [KeyboardKeyState.NOT_PRESSED]: 'keyboardDiagramAriaLabelNotPressed',
      [KeyboardKeyState.PRESSED]: 'keyboardDiagramAriaLabelPressed',
      [KeyboardKeyState.TESTED]: 'keyboardDiagramAriaLabelTested',
    };
    return this.i18n(stateStringIds[state], name);
  }

  /**
   * @param {?string} topLeftGlyph
   * @param {?string} topRightGlyph
   * @param {?string} bottomLeftGlyph
   * @param {?string} bottomRightGlyph
   * @return {boolean}
   * @private
   */
  computeShowCornerGlyphs_(
      topLeftGlyph, topRightGlyph, bottomLeftGlyph, bottomRightGlyph) {
    return !!(
        topLeftGlyph || topRightGlyph || bottomLeftGlyph || bottomRightGlyph);
  }

  /**
   * @param {?string} topRightGlyph
   * @param {?string} bottomRightGlyph
   * @return {boolean}
   * @private
   */
  computeShowSecondColumn_(topRightGlyph, bottomRightGlyph) {
    return !!(topRightGlyph || bottomRightGlyph);
  }

  /**
   * Triggers 'announce-text' event to be used in pair with cr-a11y-announcer.
   * Event provides A11Y "live" update to screen readers when a key is pressed.
   * @protected
   */
  keyboardKeyStateChanged() {
    if (this.state === KeyboardKeyState.PRESSED) {
      this.dispatchEvent(new CustomEvent('announce-text', {
        bubbles: true,
        composed: true,
        detail: {text: this.ariaLabel_},
      }));
    }
  }
}

customElements.define(KeyboardKeyElement.is, KeyboardKeyElement);
