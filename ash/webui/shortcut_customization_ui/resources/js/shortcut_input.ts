// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
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
  static get is(): string {
    return 'shortcut-input';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      shortcut: {
        type: String,
        value: '',
      },

      pendingShortcut: {
        type: String,
        value: '',
      },

      capturing: {
        type: Boolean,
        value: false,
      },
    };
  }

  private shortcut: string;
  private pendingShortcut: string;
  private capturing: boolean;

  override ready(): void {
    super.ready();
    this.addEventListener('keydown', (e) => this.onKeyDown(e));
    this.addEventListener('keyup', (e) => this.onKeyUp(e));
    this.addEventListener('focus', () => this.startCapture());
    this.addEventListener('mouseup', () => this.startCapture());
    this.addEventListener('blur', () => this.endCapture());
  }

  private startCapture(): void {
    if (this.capturing) {
      return;
    }
    this.pendingShortcut = '';
    this.shortcut = '';
    this.capturing = true;
  }

  private endCapture(): void {
    if (!this.capturing) {
      return;
    }

    this.capturing = false;
    this.pendingShortcut = '';
    this.$.input.blur();
  }

  private onKeyDown(e: KeyboardEvent): void {
    this.handleKey(e);
  }

  private onKeyUp(e: KeyboardEvent): void {
    e.preventDefault();
    e.stopPropagation();

    this.endCapture();
  }

  private computeText(): string {
    const shortcutString =
        this.capturing ? this.pendingShortcut : this.shortcut;
    return shortcutString.split('+').join(' + ');
  }

  private handleKey(e: KeyboardEvent): void {
    // While capturing, we prevent all events from bubbling, to prevent
    // shortcuts from executing and interrupting the input capture.
    e.preventDefault();
    e.stopPropagation();

    if (!this.hasValidModifiers(e)) {
      this.pendingShortcut = '';
      return;
    }
    this.pendingShortcut = this.keystrokeToString(e);

    this.shortcut = this.pendingShortcut;
  }

  /**
   * Converts a keystroke event to string form.
   * Returns the keystroke as a string.
   */
  private keystrokeToString(e: KeyboardEvent): string {
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
    if (!this.isModifierKey(e)) {
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
  private hasValidModifiers(e: KeyboardEvent): boolean {
    // Although Shift is a modifier, it cannot be a standalone modifier for a
    // shortcut.
    return e.ctrlKey || e.altKey || e.metaKey;
  }

  private isModifierKey(e: KeyboardEvent): boolean {
    return ModifierKeyCodes.includes(e.keyCode);
  }
}

customElements.define(ShortcutInputElement.is, ShortcutInputElement);