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

goog.require('KeyCode');

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
   * @return {Array<Object<{command: string, sequence: KeySequence}>>} Array of
   *     all command, key bindings.
   */
  bindings() {
    return this.bindings_;
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
   * Convenience method for getting the ChromeVox key map.
   * @return {!KeyMap} The resulting object.
   */
  static get() {
    const commandsAndKeySequences =
        /**
         * @type {Array<Object<{command: string,
         *                       sequence: KeySequence}>>}
         */
        (KeyMap.BINDINGS_);

    // Validate the type of the commandsAndKeySequences array.
    for (let i = 0; i < commandsAndKeySequences.length; i++) {
      if (commandsAndKeySequences[i].command === undefined ||
          commandsAndKeySequences[i].sequence === undefined) {
        throw new Error('Invalid key map.');
      } else {
        commandsAndKeySequences[i].sequence = /** @type {KeySequence} */
            (KeySequence.deserialize(commandsAndKeySequences[i].sequence));
      }
    }
    return new KeyMap(commandsAndKeySequences);
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

// This is intentionally not type-checked, as it is a serialized set of
// KeySequence objects.
/** @private {!Object} */
KeyMap.BINDINGS_ = [
  {
    command: 'previousObject',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT]}}
  },
  {
    command: 'previousLine',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.UP]}}
  },
  {
    command: 'nextObject',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT]}}
  },
  {
    command: 'nextLine',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN]}}
  },
  {
    command: 'nextCharacter',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT], shiftKey: [true]}}
  },
  {
    command: 'previousCharacter',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], shiftKey: [true]}}
  },
  {
    command: 'nextWord',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], shiftKey: [true]}
    }
  },
  {
    command: 'previousWord',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], shiftKey: [true]}
    }
  },
  {
    command: 'nextButton',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.B]}}
  },
  {
    command: 'previousButton',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.B], shiftKey: [true]}}
  },
  {
    command: 'nextCheckbox',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.X]}}
  },
  {
    command: 'previousCheckbox',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.X], shiftKey: [true]}}
  },
  {
    command: 'nextComboBox',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.C]}}
  },
  {
    command: 'previousComboBox',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], shiftKey: [true]}}
  },
  {
    command: 'nextEditText',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.E]}}
  },
  {
    command: 'previousEditText',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.E], shiftKey: [true]}}
  },
  {
    command: 'nextFormField',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.F]}}
  },
  {
    command: 'previousFormField',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], shiftKey: [true]}}
  },
  {
    command: 'previousGraphic',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.G], shiftKey: [true]}}
  },
  {
    command: 'nextGraphic',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.G]}}
  },
  {
    command: 'nextHeading',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.H]}}
  },
  {
    command: 'nextHeading1',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE]}}
  },
  {
    command: 'nextHeading2',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO]}}
  },
  {
    command: 'nextHeading3',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.THREE]}}
  },
  {
    command: 'nextHeading4',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR]}}
  },
  {
    command: 'nextHeading5',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE]}}
  },
  {
    command: 'nextHeading6',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX]}}
  },
  {
    command: 'previousHeading',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], shiftKey: [true]}}
  },
  {
    command: 'previousHeading1',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE], shiftKey: [true]}}
  },
  {
    command: 'previousHeading2',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO], shiftKey: [true]}}
  },
  {
    command: 'previousHeading3',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.THREE], shiftKey: [true]}}
  },
  {
    command: 'previousHeading4',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR], shiftKey: [true]}}
  },
  {
    command: 'previousHeading5',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE], shiftKey: [true]}}
  },
  {
    command: 'previousHeading6',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX], shiftKey: [true]}}
  },
  {
    command: 'nextLink',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.L]}}
  },
  {
    command: 'previousLink',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], shiftKey: [true]}}
  },
  {
    command: 'nextTable',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.T]}}
  },
  {
    command: 'previousTable',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], shiftKey: [true]}}
  },
  {
    command: 'nextVisitedLink',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.V]}}
  },
  {
    command: 'previousVisitedLink',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.V], shiftKey: [true]}}
  },
  {
    command: 'nextLandmark',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1]}}
  },
  {
    command: 'previousLandmark',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1], shiftKey: [true]}}
  },
  {
    command: 'jumpToBottom',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true]}}
  },
  {
    command: 'jumpToTop',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true]}}
  },
  {
    command: 'forceClickOnCurrentItem',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}}
  },
  {
    command: 'contextMenu',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.M]}}
  },
  {
    command: 'readFromHere',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.R]}}
  },
  {
    command: 'toggleStickyMode',
    sequence: {
      skipStripping: false,
      doubleTap: true,
      keys: {keyCode: [KeyCode.SEARCH]}
    }
  },
  {
    command: 'passThroughMode',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.ESCAPE], shiftKey: [true]}
    }
  },
  {
    command: 'toggleKeyboardHelp',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_PERIOD]}}
  },
  {
    command: 'stopSpeech',
    sequence: {
      cvoxModifier: false,
      keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]}
    }
  },
  {
    command: 'decreaseTtsRate',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_4], shiftKey: [true]}}
  },
  {
    command: 'increaseTtsRate',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_4]}}
  },
  {
    command: 'decreaseTtsPitch',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_6], shiftKey: [true]}}
  },
  {
    command: 'increaseTtsPitch',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_6]}}
  },
  {
    command: 'stopSpeech',
    sequence: {keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]}}
  },
  {
    command: 'cyclePunctuationEcho',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.P]}}
  },
  {
    command: 'showKbExplorerPage',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.K]}}
  },
  {
    command: 'cycleTypingEcho',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.T]}}
  },
  {
    command: 'showOptionsPage',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.O]}}
  },
  {
    command: 'showLogPage',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.W]}}
  },
  {
    command: 'enableLogging',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.E]}}
  },
  {
    command: 'disableLogging',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.D]}}
  },
  {
    command: 'dumpTree',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.D, KeyCode.T], ctrlKey: [true]}
    }
  },
  {
    command: 'help',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.T]}}
  },
  {
    command: 'toggleEarcons',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.E]}}
  },
  {
    command: 'speakTimeAndDate',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.D]}}
  },
  {
    command: 'readCurrentTitle',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.W]}}
  },
  {
    command: 'readCurrentURL',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.U]}}
  },
  {
    command: 'reportIssue',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.I]}}
  },
  {
    command: 'toggleSearchWidget',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_2]}}
  },
  {
    command: 'showHeadingsList',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], ctrlKey: [true]}}
  },
  {
    command: 'showFormsList',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], ctrlKey: [true]}}
  },
  {
    command: 'showLandmarksList',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1], ctrlKey: [true]}}
  },
  {
    command: 'showLinksList',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], ctrlKey: [true]}}
  },
  {
    command: 'showActionsMenu',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.A], ctrlKey: [true]}}
  },
  {
    command: 'showTablesList',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], ctrlKey: [true]}}
  },
  {
    command: 'toggleBrailleCaptions',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.B]}}
  },
  {
    command: 'toggleBrailleTable',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.G]}}
  },
  {
    command: 'viewGraphicAsBraille',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.G], altKey: [true]}}
  },
  {
    command: 'toggleSelection',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.S]}}
  },
  {
    command: 'fullyDescribe',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.K]}}
  },
  {
    command: 'previousRow',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.UP], ctrlKey: [true], altKey: [true]}
    }
  },
  {
    command: 'nextRow',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true], altKey: [true]}
    }
  },
  {
    command: 'nextCol',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], altKey: [true]}
    }
  },
  {
    command: 'previousCol',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], altKey: [true]}
    }
  },
  {
    command: 'goToRowFirstCell',
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.LEFT],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true]
      }
    }
  },
  {
    command: 'goToColFirstCell',
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.UP],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true]
      }
    }
  },
  {
    command: 'goToColLastCell',
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.DOWN],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true]
      }
    }
  },
  {
    command: 'goToFirstCell',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], altKey: [true], shiftKey: [true]}
    }
  },
  {
    command: 'goToLastCell',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], altKey: [true], shiftKey: [true]}
    }
  },
  {
    command: 'goToRowLastCell',
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.RIGHT],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true]
      }
    }
  },
  {
    command: 'previousGroup',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.UP], ctrlKey: [true]}}
  },
  {
    command: 'nextGroup',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true]}}
  },
  {
    command: 'previousSimilarItem',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.I], shiftKey: [true]}}
  },
  {
    command: 'nextSimilarItem',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.I]}}
  },
  {
    command: 'jumpToDetails',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.J]}}
  },
  {
    command: 'toggleDarkScreen',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.BRIGHTNESS_UP]}}
  },
  {
    command: 'toggleSpeechOnOrOff',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.VOLUME_MUTE]}}
  },
  {
    command: 'enableChromeVoxArcSupportForCurrentApp',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_4]}}
  },
  {
    command: 'disableChromeVoxArcSupportForCurrentApp',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_6]}}
  },
  {
    command: 'forceClickOnCurrentItem',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}, doubleTap: true}
  },
  {
    command: 'showTtsSettings',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.S]}}
  },
  {
    command: 'announceBatteryDescription',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.B]}}
  },
  {
    command: 'announceRichTextDescription',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.F]}}
  },
  {
    command: 'readPhoneticPronunciation',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.C]}}
  },
  {
    command: 'readLinkURL',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.L]}}
  },
  {
    command: 'nextList',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.J, KeyCode.L]}}
  },
  {
    command: 'previousList',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.J, KeyCode.L], shiftKey: [true]}
    }
  },
  {
    command: 'resetTextToSpeechSettings',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_5], ctrlKey: [true], shiftKey: [true]}
    }
  },
  {
    command: 'logLanguageInformationForCurrentNode',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.P, KeyCode.L]}}
  },
  {
    command: 'copy',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], ctrlKey: [true]}}
  },
];
