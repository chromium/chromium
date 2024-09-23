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
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Command} from './command.js';
import {CommandStore} from './command_store.js';
import {KeyBinding, KeySequence} from './key_sequence.js';

export class KeyMap {
  /** An array of bindings - Commands and KeySequences. */
  private bindings_: KeyBinding[];
  /**
   * Maps a command to a key. This optimizes the process of searching for a
   * key sequence when you already know the command.
   */
  private commandToKey_: Partial<Record<Command, KeySequence>> = {};

  static instance: KeyMap;

  private constructor(keyBindings: KeyBinding[]) {
    this.bindings_ = keyBindings;

    this.buildCommandToKey_();
  }

  /**
   * The number of mappings in the keymap.
   * @return The number of mappings.
   */
  length(): number {
    return this.bindings_.length;
  }

  /**
   * Returns a copy of all KeySequences in this map.
   * @return Array of all keys.
   */
  keys(): KeySequence[] {
    return this.bindings_.map(binding => binding.sequence);
  }

  /** Returns a shallow copy of the Command, KeySequence bindings. */
  bindings(): KeyBinding[] {
    return this.bindings_.slice();
  }

  /** Checks if this key map has a given binding. */
  hasBinding(command: Command, sequence: KeySequence): boolean {
    if (this.commandToKey_ != null) {
      return this.commandToKey_[command] === sequence;
    }
    return this.bindings_.some(
        b => b.command === command && b.sequence.equals(sequence));
  }

  /** Checks if this key map has a given command. */
  hasCommand(command: Command): boolean {
    if (this.commandToKey_ != null) {
      return this.commandToKey_[command] !== undefined;
    }
    return this.bindings_.some(b => b.command === command);
  }

  /** Checks if this key map has a given key. */
  hasKey(key: KeySequence): boolean {
    return this.bindings_.some(b => b.sequence.equals(key));
  }

  /** Gets a command given a key. */
  commandForKey(key: KeySequence): Command | undefined {
    return this.bindings_.find(b => b.sequence.equals(key))?.command;
  }

  /** Gets a key given a command. */
  keyForCommand(command: Command): KeySequence[] {
    if (this.commandToKey_ != null) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      return [this.commandToKey_[command]!];
    }
    return this.bindings_.filter(b => b.command === command)
        .map(b => b.sequence);
  }

  /** Convenience method for getting the ChromeVox key map. */
  static get(): KeyMap {
    if (KeyMap.instance) {
      return KeyMap.instance;
    }
    const keyBindings = CommandStore.getKeyBindings();
    KeyMap.instance = new KeyMap(keyBindings);
    return KeyMap.instance;
  }

  /** Builds the map of commands to keys. */
  private buildCommandToKey_(): void {
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

TestImportManager.exportForTesting(KeyMap);
