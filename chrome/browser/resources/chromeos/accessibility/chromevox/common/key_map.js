// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview This class provides a stable interface for initializing,
 * querying, and modifying a ChromeVox key map.
 *
 * An instance contains an object-based bi-directional mapping from key binding
 * to a function name of a user command (herein simply called a command).
 * A caller is responsible for providing a JSON keymap (a simple Object key
 * value structure), which has (key, command) key value pairs.
 *
 * To retrieve static data about user commands, see CommandStore.
 */
import {KeyCode} from '../../common/key_code.js';

import {Command} from './command.js';
import {CommandStore} from './command_store.js';
import {KeyBinding, KeySequence, SerializedKeySequence} from './key_sequence.js';

export class KeyMap {
  /**
   * @param {!Array<!KeyBinding>} keyBindings
   * @private
   */
  constructor(keyBindings) {
    /**
     * An array of bindings - Commands and KeySequences.
     * @private {!Array<!KeyBinding>}
     */
    this.bindings_ = keyBindings;

    /**
     * Maps a command to a key. This optimizes the process of searching for a
     * key sequence when you already know the command.
     * @private {Object<!Command, !KeySequence>}
     */
    this.commandToKey_ = {};
    this.buildCommandToKey_();
  }

  /**
   * The number of mappings in the keymap.
   * @return {number} The number of mappings.
   */
  length() {
    return this.bindings_.length;
  }

  /**
   * Returns a copy of all KeySequences in this map.
   * @return {!Array<!KeySequence>} Array of all keys.
   */
  keys() {
    return this.bindings_.map(binding => binding.sequence);
  }

  /**
   * Returns a shallow copy of the Command, KeySequence bindings.
   * @return {!Array<!KeyBinding>} Array of all command, key bindings.
   */
  bindings() {
    return this.bindings_.slice();
  }

  /**
   * Checks if this key map has a given binding.
   * @param {!Command} command The command.
   * @param {!KeySequence} sequence The key sequence.
   * @return {boolean} Whether the binding exists.
   */
  hasBinding(command, sequence) {
    if (this.commandToKey_ != null) {
      return this.commandToKey_[command] === sequence;
    }
    return this.bindings_.some(
        b => b.command === command && b.sequence.equals(sequence));
  }

  /**
   * Checks if this key map has a given command.
   * @param {!Command} command The command to check.
   * @return {boolean} Whether |command| has a binding.
   */
  hasCommand(command) {
    if (this.commandToKey_ != null) {
      return this.commandToKey_[command] !== undefined;
    }
    return this.bindings_.some(b => b.command === command);
  }

  /**
   * Checks if this key map has a given key.
   * @param {!KeySequence} key The key to check.
   * @return {boolean} Whether |key| has a binding.
   */
  hasKey(key) {
    return this.bindings_.some(b => b.sequence.equals(key));
  }

  /**
   * Gets a command given a key.
   * @param {!KeySequence} key The key to query.
   * @return {?Command} The command, if any.
   */
  commandForKey(key) {
    return this.bindings_.find(b => b.sequence.equals(key))?.command;
  }

  /**
   * Gets a key given a command.
   * @param {!Command} command The command to query.
   * @return {!Array<!KeySequence>} The keys associated with that command,
   * if any.
   */
  keyForCommand(command) {
    if (this.commandToKey_ != null) {
      return [this.commandToKey_[command]];
    }
    return this.bindings_.filter(b => b.command === command)
        .map(b => b.sequence);
  }

  /**
   * Convenience method for getting the ChromeVox key map.
   * @return {!KeyMap} The resulting object.
   */
  static get() {
    if (KeyMap.instance) {
      return KeyMap.instance;
    }

    const keyBindings = CommandStore.getKeyBindings();
    KeyMap.instance = new KeyMap(keyBindings);
    return KeyMap.instance;
  }

  /**
   * Builds the map of commands to keys.
   * @private
   */
  buildCommandToKey_() {
    // TODO (dtseng): What about more than one sequence mapped to the same
    // command?
    for (const binding of this.bindings_) {
      if (this.commandToKey_[binding.command] !== undefined) {
        // There's at least two key sequences mapped to the same
        // command. continue.
        continue;
      }
      this.commandToKey_[binding.command] = binding.sequence;
    }
  }
}

/** @type {KeyMap} */
KeyMap.instance;
