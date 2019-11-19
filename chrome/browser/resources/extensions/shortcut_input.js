// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';
import {hasValidModifiers, isValidKeyCode, Key, keystrokeToString} from './shortcut_util.js';

/** @enum {number} */
const ShortcutError = {
  NO_ERROR: 0,
  INCLUDE_START_MODIFIER: 1,
  TOO_MANY_MODIFIERS: 2,
  NEED_CHARACTER: 3,
};

// The UI to display and manage keyboard shortcuts set for extension commands.
Polymer({
  is: 'extensions-shortcut-input',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!KeyboardShortcutDelegate} */
    delegate: Object,

    item: {
      type: String,
      value: '',
    },

    commandName: {
      type: String,
      value: '',
    },

    shortcut: {
      type: String,
      value: '',
    },

    /** @private */
    capturing_: {
      type: Boolean,
      value: false,
    },

    /** @private {!ShortcutError} */
    error_: {
      type: Number,
      value: 0,
    },

    /** @private */
    pendingShortcut_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  ready: function() {
    const node = this.$.input;
    node.addEventListener('mouseup', this.startCapture_.bind(this));
    node.addEventListener('blur', this.endCapture_.bind(this));
    node.addEventListener('focus', this.startCapture_.bind(this));
    node.addEventListener('keydown', this.onKeyDown_.bind(this));
    node.addEventListener('keyup', this.onKeyUp_.bind(this));
  },

  /** @private */
  startCapture_: function() {
    if (this.capturing_) {
      return;
    }
    this.capturing_ = true;
    this.delegate.setShortcutHandlingSuspended(true);
  },

  /** @private */
  endCapture_: function() {
    if (!this.capturing_) {
      return;
    }
    this.pendingShortcut_ = '';
    this.capturing_ = false;
    const input = this.$.input;
    input.blur();
    input.invalid = false;
    this.delegate.setShortcutHandlingSuspended(false);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeyDown_: function(e) {
    if (e.target == this.$.clear) {
      return;
    }

    if (e.keyCode == Key.Escape) {
      if (!this.capturing_) {
        // If we're not currently capturing, allow escape to propagate.
        return;
      }
      // Otherwise, escape cancels capturing.
      this.endCapture_();
      e.preventDefault();
      e.stopPropagation();
      return;
    }
    if (e.keyCode == Key.Tab) {
      // Allow tab propagation for keyboard navigation.
      return;
    }

    if (!this.capturing_) {
      this.startCapture_();
    }

    this.handleKey_(/** @type {!KeyboardEvent} */ (e));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeyUp_: function(e) {
    // Ignores pressing 'Space' or 'Enter' on the clear button. In 'Enter's
    // case, the clear button disappears before key-up, so 'Enter's key-up
    // target becomes the input field, not the clear button, and needs to
    // be caught explicitly.
    if (e.target == this.$.clear || e.key == 'Enter') {
      return;
    }

    if (e.keyCode == Key.Escape || e.keyCode == Key.Tab) {
      return;
    }

    this.handleKey_(/** @type {!KeyboardEvent} */ (e));
  },

  /**
   * @param {!ShortcutError} error
   * @param {string} includeStartModifier
   * @param {string} tooManyModifiers
   * @param {string} needCharacter
   * @return {string} UI string.
   * @private
   */
  getErrorString_: function(
      error, includeStartModifier, tooManyModifiers, needCharacter) {
    if (error == ShortcutError.TOO_MANY_MODIFIERS) {
      return tooManyModifiers;
    }
    if (error == ShortcutError.NEED_CHARACTER) {
      return needCharacter;
    }
    return includeStartModifier;
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  handleKey_: function(e) {
    // While capturing, we prevent all events from bubbling, to prevent
    // shortcuts lacking the right modifier (F3 for example) from activating
    // and ending capture prematurely.
    e.preventDefault();
    e.stopPropagation();

    // We don't allow both Ctrl and Alt in the same keybinding.
    // TODO(devlin): This really should go in hasValidModifiers,
    // but that requires updating the existing page as well.
    if (e.ctrlKey && e.altKey) {
      this.error_ = ShortcutError.TOO_MANY_MODIFIERS;
      this.$.input.invalid = true;
      return;
    }
    if (!hasValidModifiers(e)) {
      this.pendingShortcut_ = '';
      this.error_ = ShortcutError.INCLUDE_START_MODIFIER;
      this.$.input.invalid = true;
      return;
    }
    this.pendingShortcut_ = keystrokeToString(e);
    if (!isValidKeyCode(e.keyCode)) {
      this.error_ = ShortcutError.NEED_CHARACTER;
      this.$.input.invalid = true;
      return;
    }
    this.$.input.invalid = false;

    this.commitPending_();
    this.endCapture_();
  },

  /** @private */
  commitPending_: function() {
    this.shortcut = this.pendingShortcut_;
    this.delegate.updateExtensionCommandKeybinding(
        this.item, this.commandName, this.shortcut);
  },

  /**
   * @return {string} The text to be displayed in the shortcut field.
   * @private
   */
  computeText_: function() {
    const shortcutString =
        this.capturing_ ? this.pendingShortcut_ : this.shortcut;
    return shortcutString.split('+').join(' + ');
  },

  /**
   * @return {boolean} Whether the clear button is hidden.
   * @private
   */
  computeClearHidden_: function() {
    // We don't want to show the clear button if the input is currently
    // capturing a new shortcut or if there is no shortcut to clear.
    return this.capturing_ || !this.shortcut;
  },

  /** @private */
  onClearTap_: function() {
    assert(this.shortcut);

    this.pendingShortcut_ = '';
    this.commitPending_();
    this.endCapture_();
  },
});
