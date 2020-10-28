// Copyright 2014 The Chromium Authors. All rights reserved.
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
 * Due to execution of user commands within the content script, the function
 * name of the command is not explicitly checked within the background page via
 * Closure. Any errors would only be caught at runtime.
 *
 * To retrieve static data about user commands, see both CommandStore and
 * UserCommands.
 */

goog.provide('KeyMap');

// TODO(dtseng): Only needed for sticky mode.
goog.require('KeyUtil');

KeyMap = class {
  /**
   * @param {Array<Object<{command: string, sequence: KeySequence}>>}
   * commandsAndKeySequences An array of pairs - KeySequences and commands.
   */
  constructor(commandsAndKeySequences) {
    /**
     * An array of bindings - commands and KeySequences.
     * @type {Array<Object<{command: string, sequence: KeySequence}>>}
     * @private
     */
    this.bindings_ = commandsAndKeySequences;

    /**
     * Maps a command to a key. This optimizes the process of searching for a
     * key sequence when you already know the command.
     * @type {Object<KeySequence>}
     * @private
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
   * @return {Array<KeySequence>} Array of all keys.
   */
  keys() {
    return this.bindings_.map(function(binding) {
      return binding.sequence;
    });
  }

  /**
   * Returns a collection of command, KeySequence bindings.
   * @return {Array<Object<KeySequence>>} Array of all command, key bindings.
   * @suppress {checkTypes} inconsistent return type
   * found   : (Array<(Object<{command: string,
   *                             sequence: (KeySequence|null)}>|null)>|null)
   * required: (Array<(Object<(KeySequence|null)>|null)>|null)
   */
  bindings() {
    return this.bindings_;
  }

  /**
   * This method is called when KeyMap instances are stringified via
   * JSON.stringify.
   * @return {string} The JSON representation of this instance.
   */
  toJSON() {
    return JSON.stringify({bindings: this.bindings_});
  }

  /**
   * Writes to local storage.
   */
  toLocalStorage() {
    localStorage['keyBindings'] = this.toJSON();
  }

  /**
   * Checks if this key map has a given binding.
   * @param {string} command The command.
   * @param {KeySequence} sequence The key sequence.
   * @return {boolean} Whether the binding exists.
   */
  hasBinding(command, sequence) {
    if (this.commandToKey_ != null) {
      return this.commandToKey_[command] === sequence;
    } else {
      for (let i = 0; i < this.bindings_.length; i++) {
        const binding = this.bindings_[i];
        if (binding.command === command && binding.sequence === sequence) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Checks if this key map has a given command.
   * @param {string} command The command to check.
   * @return {boolean} Whether 'command' has a binding.
   */
  hasCommand(command) {
    if (this.commandToKey_ != null) {
      return this.commandToKey_[command] !== undefined;
    } else {
      for (let i = 0; i < this.bindings_.length; i++) {
        const binding = this.bindings_[i];
        if (binding.command === command) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Checks if this key map has a given key.
   * @param {KeySequence} key The key to check.
   * @return {boolean} Whether 'key' has a binding.
   */
  hasKey(key) {
    for (let i = 0; i < this.bindings_.length; i++) {
      const binding = this.bindings_[i];
      if (binding.sequence.equals(key)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Gets a command given a key.
   * @param {KeySequence} key The key to query.
   * @return {?string} The command, if any.
   */
  commandForKey(key) {
    if (key != null) {
      for (let i = 0; i < this.bindings_.length; i++) {
        const binding = this.bindings_[i];
        if (binding.sequence.equals(key)) {
          return binding.command;
        }
      }
    }
    return null;
  }

  /**
   * Gets a key given a command.
   * @param {string} command The command to query.
   * @return {!Array<KeySequence>} The keys associated with that command,
   * if any.
   */
  keyForCommand(command) {
    let keySequenceArray;
    if (this.commandToKey_ != null) {
      return [this.commandToKey_[command]];
    } else {
      keySequenceArray = [];
      for (let i = 0; i < this.bindings_.length; i++) {
        const binding = this.bindings_[i];
        if (binding.command === command) {
          keySequenceArray.push(binding.sequence);
        }
      }
    }
    return (keySequenceArray.length > 0) ? keySequenceArray : [];
  }

  /**
   * Merges an input map with this one. The merge preserves this instance's
   * mappings. It only adds new bindings if there isn't one already.
   * If either the incoming binding's command or key exist in this, it will be
   * ignored.
   * @param {!KeyMap} inputMap The map to merge with this.
   * @return {boolean} True if there were no merge conflicts.
   */
  merge(inputMap) {
    const keys = inputMap.keys();
    let cleanMerge = true;
    for (let i = 0; i < keys.length; ++i) {
      const key = keys[i];
      const command = inputMap.commandForKey(key);
      if (command === 'toggleStickyMode') {
        // TODO(dtseng): More uglyness because of sticky key.
        continue;
      } else if (
          key && command && !this.hasKey(key) && !this.hasCommand(command)) {
        this.bind_(command, key);
      } else {
        cleanMerge = false;
      }
    }
    return cleanMerge;
  }

  /**
   * Changes an existing key binding to a new key. If the key is already bound
   * to a command, the rebind will fail.
   * @param {string} command The command to set.
   * @param {KeySequence} newKey The new key to assign it to.
   * @return {boolean} Whether the rebinding succeeds.
   */
  rebind(command, newKey) {
    if (this.hasCommand(command) && !this.hasKey(newKey)) {
      this.bind_(command, newKey);
      return true;
    }
    return false;
  }

  /**
   * Changes a key binding. Any existing bindings to the given key will be
   * deleted. Use this.rebind to have non-overwrite behavior.
   * @param {string} command The command to set.
   * @param {KeySequence} newKey The new key to assign it to.
   * @private
   */
  bind_(command, newKey) {
    // TODO(dtseng): Need unit test to ensure command is valid for every *.json
    // keymap.
    let bound = false;
    for (let i = 0; i < this.bindings_.length; i++) {
      const binding = this.bindings_[i];
      if (binding.command === command) {
        // Replace the key with the new key.
        delete binding.sequence;
        binding.sequence = newKey;
        if (this.commandToKey_ != null) {
          this.commandToKey_[binding.command] = newKey;
        }
        bound = true;
      }
    }
    if (!bound) {
      const binding = {command, 'sequence': newKey};
      this.bindings_.push(binding);
      this.commandToKey_[binding.command] = binding.sequence;
    }
  }

  /**
   * Convenience method for getting a default key map.
   * @return {!KeyMap} The default key map.
   */
  static fromDefaults() {
    return /** @type {!KeyMap} */ (KeyMap.fromPath(
        KeyMap.KEYMAP_PATH + KeyMap.AVAILABLE_MAP_INFO['keymap_default'].file));
  }

  /**
   * Convenience method for creating a key map based on a JSON (key, value)
   * Object where the key is a literal keyboard string and value is a command
   * string.
   * @param {string} json The JSON.
   * @return {KeyMap} The resulting object; null if unable to parse.
   */
  static fromJSON(json) {
    let commandsAndKeySequences = null;
    try {
      commandsAndKeySequences =
          /**
           * @type {Array<Object<{command: string,
           *                       sequence: KeySequence}>>}
           */
          (JSON.parse(json).bindings);
    } catch (e) {
      console.error('Failed to load key map from JSON');
      console.error(e);
      return null;
    }

    // Validate the type of the commandsAndKeySequences array.
    if (typeof (commandsAndKeySequences) !== 'object') {
      return null;
    }
    for (let i = 0; i < commandsAndKeySequences.length; i++) {
      if (commandsAndKeySequences[i].command === undefined ||
          commandsAndKeySequences[i].sequence === undefined) {
        return null;
      } else {
        commandsAndKeySequences[i].sequence = /** @type {KeySequence} */
            (KeySequence.deserialize(commandsAndKeySequences[i].sequence));
      }
    }
    return new KeyMap(commandsAndKeySequences);
  }

  /**
   * Convenience method for creating a map local storage.
   * @return {KeyMap} A map that reads from local storage.
   */
  static fromLocalStorage() {
    if (localStorage['keyBindings']) {
      return KeyMap.fromJSON(localStorage['keyBindings']);
    }
    return null;
  }

  /**
   * Convenience method for creating a KeyMap based on a path.
   * Warning: you should only call this within a background page context.
   * @param {string} path A valid path of the form
   * chromevox/background/keymaps/*.json.
   * @return {KeyMap} A valid KeyMap object; null on error.
   */
  static fromPath(path) {
    return KeyMap.fromJSON(KeyMap.readJSON_(path));
  }

  /**
   * Convenience method for getting a currently selected key map.
   * @return {!KeyMap} The currently selected key map.
   */
  static fromCurrentKeyMap() {
    const map = localStorage['currentKeyMap'];
    if (map && KeyMap.AVAILABLE_MAP_INFO[map]) {
      return /** @type {!KeyMap} */ (KeyMap.fromPath(
          KeyMap.KEYMAP_PATH + KeyMap.AVAILABLE_MAP_INFO[map].file));
    } else {
      return KeyMap.fromDefaults();
    }
  }

  /**
   * Takes a path to a JSON file and returns a JSON Object.
   * @param {string} path Contains the path to a JSON file.
   * @return {string} JSON.
   * @private
   * @suppress {missingProperties}
   */
  static readJSON_(path) {
    const url = chrome.extension.getURL(path);
    if (!url) {
      throw 'Invalid path: ' + path;
    }

    const xhr = new XMLHttpRequest();
    xhr.open('GET', url, false);
    xhr.send();
    return xhr.responseText;
  }

  /**
   * Resets the default modifier keys.
   * TODO(dtseng): Move elsewhere when we figure out our localStorage story.
   */
  resetModifier() {
    localStorage['cvoxKey'] = ChromeVox.modKeyStr;
  }

  /**
   * Builds the map of commands to keys.
   * @private
   */
  buildCommandToKey_() {
    // TODO (dtseng): What about more than one sequence mapped to the same
    // command?
    for (let i = 0; i < this.bindings_.length; i++) {
      const binding = this.bindings_[i];
      if (this.commandToKey_[binding.command] !== undefined) {
        // There's at least two key sequences mapped to the same
        // command. continue.
        continue;
      }
      this.commandToKey_[binding.command] = binding.sequence;
    }
  }
};


/**
 * Path to dir containing ChromeVox keymap json definitions.
 * @type {string}
 * @const
 */
KeyMap.KEYMAP_PATH = 'chromevox/background/keymaps/';


/**
 * An array of available key maps sorted by priority.
 * (The first map is the default, the last is the least important).
 * TODO(dtseng): Not really sure this belongs here, but it doesn't seem to be
 * user configurable, so it doesn't make sense to json-stringify it.
 * Should have class to siwtch among and manage multiple key maps.
 * @type {Object<Object<string>>}
 * @const
 */
KeyMap.AVAILABLE_MAP_INFO = {
  'keymap_default': {'file': 'default_keymap.json'}
};


/**
 * The index of the default key map info in KeyMap.AVAIABLE_KEYMAP_INFO.
 * @type {number}
 * @const
 */
KeyMap.DEFAULT_KEYMAP = 0;
