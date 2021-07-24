// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js'

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';

// Modifier values are based off of ui::Accelerator. Must be kept in sync with
// ui::Accelerator.
export const ModifierKeys = {
  SHIFT: /*EF_SHIFT_DOWN=*/ 1 << 1,
  CONTROL: /*EF_CONTROL_DOWN=*/ 1 << 2,
  ALT: /*EF_ALT_DOWN=*/ 1 << 3,
  COMMAND: /*EF_COMMAND_DOWN=*/ 1 << 4,
}

const ModifierRawKeys = [
  /*Shift=*/16,
  /*Alt=*/17,
  /*Ctrl=*/18,
  /*MetaLeft=*/91,
  /*MetaRight=*/92,
]

const KeyState = {
  NOT_SELECTED: 'not-selected',
  MODIFIER: 'modifier-selected',
  ALPHANUMERIC: 'alpha-numeric-selected',
}

/**
 * Returns the converted modifier flag as a readable string.
 * TODO(jimmyxgong): Localize, replace with icon, or update strings.
 * @param {number} modifier
 * @return {string}
 */
function GetModifierString(modifier) {
  switch(modifier) {
    case ModifierKeys.SHIFT:
      return 'shift';
    case ModifierKeys.CONTROL:
      return 'ctrl';
    case ModifierKeys.ALT:
      return 'alt';
    case ModifierKeys.COMMAND:
      return 'meta';
    default:
      assertNotReached();
      return '';
  }
}

/**
 * @fileoverview
 * 'accelerator-view' is wrapper component for an accelerator. It maintains both
 * the read-only and editable state of an accelerator.
 * TODO(jimmyxgong): Implement the edit mode.
 */
export class AcceleratorViewElement extends PolymerElement {
  static get is() {
    return 'accelerator-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * TODO(jimmyxgong): Update this type to be the actual mojom::accelerator.
       * @type{!Object}
       */
      accelerator: {
        type: Object,
        value: {},
      },

      /**
       * TODO(jimmyxgong): Update this type to be the actual mojom::accelerator.
       * @type{!Object}
       */
      pendingAccelerator_: {
        type: Object,
        value: {modifiers: 0, key: ''},
      },

      isEditable: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onIsEditableChanged_',
      },

      /**
       * @type{!Array<string>}
       * @private
       */
      modifiers_: {
        type: Array,
        computed: 'getModifiers_(accelerator)',
      },

      /** @private */
      isCapturing_: {
        type: Boolean,
        value: false,
      }
    }
  }

  /**
   * @return {!Array<string>}
   * @private
   */
  getModifiers_() {
    let modifiers = [];
    for (const key in ModifierKeys) {
      const modifier = ModifierKeys[key];
      if (this.accelerator.modifiers & modifier) {
        modifiers.push(GetModifierString(modifier));
      }
    }
    return modifiers;
  }

  /** @private */
  onIsEditableChanged_() {
    if (this.isEditable) {
      this.registerKeyEventListeners_();
      return;
    }
    this.unregisterKeyEventListeners_();
  }

  /** @private */
  registerKeyEventListeners_() {
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('keyup', this.onKeyUp_.bind(this));
    this.addEventListener('focus', this.startCapture_.bind(this));
    this.addEventListener('mouseup', this.startCapture_.bind(this));
    this.addEventListener('blur', this.endCapture_.bind(this));
    this.$.container.focus();
  }

  /** @private */
  unregisterKeyEventListeners_() {
    this.removeEventListener('keydown', this.onKeyDown_.bind(this));
    this.removeEventListener('keyup', this.onKeyUp_.bind(this));
    this.removeEventListener('focus', this.startCapture_.bind(this));
    this.removeEventListener('mouseup', this.startCapture_.bind(this));
    this.removeEventListener('blur', this.endCapture_.bind(this));
  }


  /** @private */
  startCapture_() {
    if (this.isCapturing_) {
      return;
    }
    // TODO(jimmyxgong): Update this to the proper mojom::Accelerator type
    // Disable ChromeOS accelerator handler when starting input capture.
    this.pendingAccelerator_ = {modifiers: 0, key: ''};
    this.isCapturing_ = true;
  }

  /** @private */
  endCapture_() {
    if (!this.isCapturing_) {
      return;
    }

    this.isCapturing_ = false;
    this.pendingAccelerator_ = {modifiers: 0, key: ''};
    this.isEditable = false;
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
    // TODO(jimmyxgong): Check for errors e.g. accelerator conflicts.
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
      // TODO(jimmyxgong): Fire events for error handling, e.g. Shift cannot be
      // the only modifier.
      this.pendingAccelerator_ = {modifiers: 0, key: ''};
      return;
    }
    this.pendingAccelerator_ = this.keystrokeToAccelerator_(e);
  }

  /**
   * Converts a keystroke event to an Accelerator Object.
   * TODO(jimmyxgong): Convert return type to proper mojom::Accelerator type.
   * @param {!KeyboardEvent} e
   * @return {!Object} The keystroke as an Acccelerator object.
   * @private
   */
  keystrokeToAccelerator_(e) {
    const output = {modifiers: 0, key: ''};
    if (e.metaKey) {
      output.modifiers = output.modifiers | ModifierKeys.COMMAND;
    }
    if (e.ctrlKey) {
      output.modifiers = output.modifiers | ModifierKeys.CONTROL;
    }
    if (e.altKey) {
      output.modifiers = output.modifiers | ModifierKeys.ALT;
    }
    // Shift key isn't registered as a modifier unless a non-modifer key is
    // pressed in conjunction with the keystroke.
    if (e.key == "Shift" || e.shiftKey) {
      output.modifiers = output.modifiers | ModifierKeys.SHIFT;
    }

    // Only add non-modifier keys as the pending key.
    if (!this.isModifierKey_(e)) {
      output.key = e.key;
    }

    return output;
  }

  /**
   * @param {!KeyboardEvent} e
   * @return {boolean}
   * @private
   */
  isModifierKey_(e) {
    return ModifierRawKeys.includes(e.keyCode);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @private
   */
  getCtrlState_() {
    return this.getModifierState_(ModifierKeys.CONTROL);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @private
   */
  getAltState_() {
    return this.getModifierState_(ModifierKeys.ALT);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @private
   */
  getShiftState_() {
    return this.getModifierState_(ModifierKeys.SHIFT);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @private
   */
  getSearchState_() {
    return this.getModifierState_(ModifierKeys.COMMAND);
  }

  /**
   * @param {number} modifier
   * @return {string} The specified CSS state of the modifier key element.
   * @private
   */
  getModifierState_(modifier) {
    if (this.pendingAccelerator_.modifiers & modifier) {
      return KeyState.MODIFIER;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * @return {string} The specified CSS state of the pending key element.
   * @private
   */
  getPendingKeyState_() {
    if (this.pendingAccelerator_.key != '') {
      return KeyState.ALPHANUMERIC;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * @return {string} The specified key to display.
   * @private
   */
  getPendingKey_() {
    if (this.pendingAccelerator_.key != '') {
      return this.pendingAccelerator_.key.toLowerCase();
    }
    // TODO(jimmyxgong): Reset to a localized default empty state.
    return 'key';
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
}

customElements.define(AcceleratorViewElement.is, AcceleratorViewElement);
