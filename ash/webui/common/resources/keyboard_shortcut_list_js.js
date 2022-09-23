// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file is deprecated and retained only for legacy JS code that is
// still using closure compiler for type checking. New code should use
// keyboard_shortcut_list.ts.

/** This is used to identify keyboard shortcuts. */
class KeyboardShortcut {
  /**
   * @param {string} shortcut The text used to describe the keys for this
   *     keyboard shortcut.
   */
  constructor(shortcut) {
    /** @private {boolean} */
    this.useKeyCode_ = false;

    /** @private {Object} */
    this.mods_ = {};

    /** @private {?string} */
    this.key_ = null;

    /** @private {?number} */
    this.keyCode_ = null;

    shortcut.split('|').forEach((part) => {
      const partLc = part.toLowerCase();
      switch (partLc) {
        case 'alt':
        case 'ctrl':
        case 'meta':
        case 'shift':
          this.mods_[partLc + 'Key'] = true;
          break;
        default:
          if (this.key_) {
            throw Error('Invalid shortcut');
          }
          this.key_ = part;
          // For single key alpha shortcuts use event.keyCode rather than
          // event.key to match how chrome handles shortcuts and allow
          // non-english language input to work.
          if (part.match(/^[a-z]$/)) {
            this.useKeyCode_ = true;
            this.keyCode_ = part.toUpperCase().charCodeAt(0);
          }
      }
    });
  }

  /**
   * Whether the keyboard shortcut object matches a keyboard event.
   * @param {!Event} e The keyboard event object.
   * @return {boolean} Whether we found a match or not.
   */
  matchesEvent(e) {
    if ((this.useKeyCode_ && e.keyCode === this.keyCode_) ||
        e.key === this.key_) {
      // All keyboard modifiers need to match.
      const mods = this.mods_;
      return ['altKey', 'ctrlKey', 'metaKey', 'shiftKey'].every(function(k) {
        return e[k] === !!mods[k];
      });
    }
    return false;
  }
}

/** A list of keyboard shortcuts which all perform one command. */
export class KeyboardShortcutList {
  /**
   * @param {string} shortcuts Text-based representation of one or more
   *     keyboard shortcuts, separated by spaces.
   */
  constructor(shortcuts) {
    this.shortcuts_ = shortcuts.split(/\s+/).map(function(shortcut) {
      return new KeyboardShortcut(shortcut);
    });
  }

  /**
   * Returns true if any of the keyboard shortcuts in the list matches a
   * keyboard event.
   * @param {!Event} e
   * @return {boolean}
   */
  matchesEvent(e) {
    return this.shortcuts_.some(function(keyboardShortcut) {
      return keyboardShortcut.matchesEvent(e);
    });
  }
}

