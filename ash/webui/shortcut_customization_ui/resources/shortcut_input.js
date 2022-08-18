// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './shortcut_input.html.js';

const ModifierKeyCodes = [
  /*Shift=*/ 16,
  /*Alt=*/ 17,
  /*Ctrl=*/ 18,
  /*MetaLeft=*/ 91,
  /*MetaRight=*/ 92,
];

/**
 * @fileoverview
 * 'shortcut-input' is the shortcut input element that consumes user inputs
 * and displays the shortcut.
 */
export class ShortcutInputElement extends PolymerElement {
  static get is() {
    return 'shortcut-input';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @private */
      shortcut_: {
        type: String,
        value: '',
      },

      /** @private */
      pendingShortcut_: {
        type: String,
        value: '',
      },

      /** @private */
      capturing_: {
        type: Boolean,
        value: false,
      },
    };
  }

  ready() {
    super.ready();
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('keyup', this.onKeyUp_.bind(this));
    this.addEventListener('focus', this.startCapture_.bind(this));
    this.addEventListener('mouseup', this.startCapture_.bind(this));
    this.addEventListener('blur', this.endCapture_.bind(this));
  }

  /** @private */
  startCapture_() {
    if (this.capturing_) {
      return;
    }
    this.pendingShortcut_ = '';
    this.shortcut_ = '';
    this.capturing_ = true;
  }

  /** @private */
  endCapture_() {
    if (!this.capturing_) {
      return;
    }

    this.capturing_ = false;
    this.pendingShortcut_ = '';
    this.$.input.blur();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeyDown_(e) {
    this.handleKey_(/** @type {!KeyboardEvent}*/ (e));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeyUp_(e) {
    e.preventDefault();
    e.stopPropagation();

    this.endCapture_();
  }

  /**
   * @return {string}
   * @private
   */
  computeText_() {
    const shortcutString =
        this.capturing_ ? this.pendingShortcut_ : this.shortcut_;
    return shortcutString.split('+').join(' + ');
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  handleKey_(e) {
    // While capturing, we prevent all events from bubbling, to prevent
    // shortcuts from executing and interrupting the input capture.
    e.preventDefault();
    e.stopPropagation();

    if (!this.hasValidModifiers_(e)) {
      this.pendingShortcut_ = '';
      return;
    }
    this.pendingShortcut_ = this.keystrokeToString_(e);

    this.shortcut_ = this.pendingShortcut_;
  }

  /**
   * Converts a keystroke event to string form.
   * @param {!KeyboardEvent} e
   * @return {string} The keystroke as a string.
   * @private
   */
  keystrokeToString_(e) {
    const output = [];
    if (e.metaKey) {
      output.push('Search');
    }
    if (e.ctrlKey) {
      output.push('Ctrl');
    }
    if (e.altKey) {
      output.push('Alt');
    }
    if (e.shiftKey) {
      output.push('Shift');
    }

    // Only add non-modifier keys, otherwise we will double capture the modifier
    // keys.
    if (!this.isModifierKey_(e)) {
      // TODO(jimmyxgong): update this to show only the DomKey.
      // Displays in the format: (DomKey)(V-Key)(DomCode), e.g.
      // ([)(219)(BracketLeft).
      output.push('(' + e.key + ')' + '(' + e.keyCode + ')' +
          '(' + e.code + ')');
    }

    return output.join('+');
  }

  /**
   * Returns true if the event has valid modifiers.
   * @param {!KeyboardEvent} e The keyboard event to consider.
   * @return {boolean} True if the event is valid.
   * @private
   */
  hasValidModifiers_(e) {
    // Although Shift is a modifier, it cannot be a standalone modifier for a
    // shortcut.
    return e.ctrlKey || e.altKey || e.metaKey;
  }

  /**
   * @param {!KeyboardEvent} e
   * @return {boolean}
   * @private
   */
  isModifierKey_(e) {
    return ModifierKeyCodes.includes(e.keyCode);
  }
}

customElements.define(ShortcutInputElement.is, ShortcutInputElement);