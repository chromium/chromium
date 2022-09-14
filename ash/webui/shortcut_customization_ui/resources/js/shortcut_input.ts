// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './shortcut_input.html.js';

export interface ShortcutInputElement {
  $: {
    input: CrInputElement,
  };
}

enum AllowedModifierKeyCodes {
  SHIFT = 16,
  ALT = 17,
  CTRL = 18,
  META_LEFT = 91,
  META_RIGHT = 92,
}

export const ModifierKeyCodes: AllowedModifierKeyCodes[] = [
  AllowedModifierKeyCodes.SHIFT,
  AllowedModifierKeyCodes.ALT,
  AllowedModifierKeyCodes.CTRL,
  AllowedModifierKeyCodes.META_LEFT,
  AllowedModifierKeyCodes.META_RIGHT,
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
      shortcut_: {
        type: String,
        value: '',
      },

      pendingShortcut_: {
        type: String,
        value: '',
      },

      capturing_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private shortcut_: string;
  private pendingShortcut_: string;
  private capturing_: boolean;

  override ready() {
    super.ready();
    this.addEventListener('keydown', (e) => this.onKeyDown_(e));
    this.addEventListener('keyup', (e) => this.onKeyUp_(e));
    this.addEventListener('focus', () => this.startCapture_());
    this.addEventListener('mouseup', () => this.startCapture_());
    this.addEventListener('blur', () => this.endCapture_());
  }

  private startCapture_() {
    if (this.capturing_) {
      return;
    }
    this.pendingShortcut_ = '';
    this.shortcut_ = '';
    this.capturing_ = true;
  }

  private endCapture_() {
    if (!this.capturing_) {
      return;
    }

    this.capturing_ = false;
    this.pendingShortcut_ = '';
    this.$.input.blur();
  }

  private onKeyDown_(e: KeyboardEvent) {
    this.handleKey_(e);
  }

  private onKeyUp_(e: KeyboardEvent) {
    e.preventDefault();
    e.stopPropagation();

    this.endCapture_();
  }

  private computeText_(): string {
    const shortcutString =
        this.capturing_ ? this.pendingShortcut_ : this.shortcut_;
    return shortcutString.split('+').join(' + ');
  }

  private handleKey_(e: KeyboardEvent) {
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
   * Returns the keystroke as a string.
   */
  private keystrokeToString_(e: KeyboardEvent): string {
    const output: string[] = [];
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
      output.push(
          '(' + e.key + ')' +
          '(' + e.keyCode + ')' +
          '(' + e.code + ')');
    }

    return output.join('+');
  }

  /** Returns true if the event has valid modifiers. */
  private hasValidModifiers_(e: KeyboardEvent): boolean {
    // Although Shift is a modifier, it cannot be a standalone modifier for a
    // shortcut.
    return e.ctrlKey || e.altKey || e.metaKey;
  }

  private isModifierKey_(e: KeyboardEvent): boolean {
    return ModifierKeyCodes.includes(e.keyCode);
  }
}

customElements.define(ShortcutInputElement.is, ShortcutInputElement);