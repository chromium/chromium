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
    'command': 'previousObject',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [37]}}
  },
  {
    'command': 'previousLine',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [38]}}
  },
  {
    'command': 'nextObject',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [39]}}
  },
  {
    'command': 'nextLine',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [40]}}
  },
  {
    'command': 'nextCharacter',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [39], 'shiftKey': [true]}}
  },
  {
    'command': 'previousCharacter',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [37], 'shiftKey': [true]}}
  },
  {
    'command': 'nextWord',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [39], 'ctrlKey': [true], 'shiftKey': [true]}
    }
  },
  {
    'command': 'previousWord',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [37], 'ctrlKey': [true], 'shiftKey': [true]}
    }
  },
  {
    'command': 'nextButton',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [66]}}
  },
  {
    'command': 'previousButton',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [66], 'shiftKey': [true]}}
  },
  {
    'command': 'nextCheckbox',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [88]}}
  },
  {
    'command': 'previousCheckbox',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [88], 'shiftKey': [true]}}
  },
  {
    'command': 'nextComboBox',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [67]}}
  },
  {
    'command': 'previousComboBox',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [67], 'shiftKey': [true]}}
  },
  {
    'command': 'nextEditText',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [69]}}
  },
  {
    'command': 'previousEditText',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [69], 'shiftKey': [true]}}
  },
  {
    'command': 'nextFormField',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [70]}}
  },
  {
    'command': 'previousFormField',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [70], 'shiftKey': [true]}}
  },
  {
    'command': 'previousGraphic',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [71], 'shiftKey': [true]}}
  },
  {
    'command': 'nextGraphic',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [71]}}
  },
  {
    'command': 'nextHeading',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [72]}}
  },
  {
    'command': 'nextHeading1',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [49]}}
  },
  {
    'command': 'nextHeading2',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [50]}}
  },
  {
    'command': 'nextHeading3',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [51]}}
  },
  {
    'command': 'nextHeading4',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [52]}}
  },
  {
    'command': 'nextHeading5',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [53]}}
  },
  {
    'command': 'nextHeading6',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [54]}}
  },
  {
    'command': 'previousHeading',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [72], 'shiftKey': [true]}}
  },
  {
    'command': 'previousHeading1',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [49], 'shiftKey': [true]}}
  },
  {
    'command': 'previousHeading2',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [50], 'shiftKey': [true]}}
  },
  {
    'command': 'previousHeading3',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [51], 'shiftKey': [true]}}
  },
  {
    'command': 'previousHeading4',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [52], 'shiftKey': [true]}}
  },
  {
    'command': 'previousHeading5',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [53], 'shiftKey': [true]}}
  },
  {
    'command': 'previousHeading6',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [54], 'shiftKey': [true]}}
  },
  {
    'command': 'nextLink',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [76]}}
  },
  {
    'command': 'previousLink',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [76], 'shiftKey': [true]}}
  },
  {
    'command': 'nextTable',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [84]}}
  },
  {
    'command': 'previousTable',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [84], 'shiftKey': [true]}}
  },
  {
    'command': 'nextVisitedLink',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [86]}}
  },
  {
    'command': 'previousVisitedLink',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [86], 'shiftKey': [true]}}
  },
  {
    'command': 'nextLandmark',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [186]}}
  },
  {
    'command': 'previousLandmark',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [186], 'shiftKey': [true]}}
  },
  {
    'command': 'jumpToBottom',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [39], 'ctrlKey': [true]}}
  },
  {
    'command': 'jumpToTop',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [37], 'ctrlKey': [true]}}
  },
  {
    'command': 'forceClickOnCurrentItem',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [32]}}
  },
  {
    'command': 'contextMenu',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [77]}}
  },
  {
    'command': 'readFromHere',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [82]}}
  },
  {
    'command': 'toggleStickyMode',
    'sequence':
        {'skipStripping': false, 'doubleTap': true, 'keys': {'keyCode': [91]}}
  },
  {
    'command': 'passThroughMode',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [27], 'shiftKey': [true]}}
  },
  {
    'command': 'toggleKeyboardHelp',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [190]}}
  },
  {
    'command': 'stopSpeech',
    'sequence':
        {'cvoxModifier': false, 'keys': {'ctrlKey': [true], 'keyCode': [17]}}
  },
  {
    'command': 'decreaseTtsRate',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [219], 'shiftKey': [true]}}
  },
  {
    'command': 'increaseTtsRate',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [219]}}
  },
  {
    'command': 'decreaseTtsPitch',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [221], 'shiftKey': [true]}}
  },
  {
    'command': 'increaseTtsPitch',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [221]}}
  },
  {
    'command': 'stopSpeech',
    'sequence': {'keys': {'ctrlKey': [true], 'keyCode': [17]}}
  },
  {
    'command': 'cyclePunctuationEcho',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 80]}}
  },
  {
    'command': 'showKbExplorerPage',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 75]}}
  },
  {
    'command': 'cycleTypingEcho',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 84]}}
  },
  {
    'command': 'showOptionsPage',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 79]}}
  },
  {
    'command': 'showLogPage',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 87]}}
  },
  {
    'command': 'enableLogging',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 69]}}
  },
  {
    'command': 'disableLogging',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 68]}}
  },
  {
    'command': 'dumpTree',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [68, 84], 'ctrlKey': [true]}}
  },
  {
    'command': 'help',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 84]}}
  },
  {
    'command': 'showNextUpdatePage',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 78]}}
  },
  {
    'command': 'toggleEarcons',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 69]}}
  },
  {
    'command': 'speakTimeAndDate',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 68]}}
  },
  {
    'command': 'readCurrentTitle',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 87]}}
  },
  {
    'command': 'readCurrentURL',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 85]}}
  },
  {
    'command': 'reportIssue',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 73]}}
  },
  {
    'command': 'toggleSearchWidget',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [191]}}
  },
  {
    'command': 'showHeadingsList',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [72], 'ctrlKey': [true]}}
  },
  {
    'command': 'showFormsList',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [70], 'ctrlKey': [true]}}
  },
  {
    'command': 'showLandmarksList',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [186], 'ctrlKey': [true]}}
  },
  {
    'command': 'showLinksList',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [76], 'ctrlKey': [true]}}
  },
  {
    'command': 'showTablesList',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [84], 'ctrlKey': [true]}}
  },
  {
    'command': 'toggleBrailleCaptions',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 66]}}
  },
  {
    'command': 'toggleBrailleTable',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 71]}}
  },
  {
    'command': 'viewGraphicAsBraille',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [71], 'altKey': [true]}}
  },
  {
    'command': 'toggleSelection',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [83]}}
  },
  {
    'command': 'fullyDescribe',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [75]}}
  },
  {
    'command': 'previousRow',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [38], 'ctrlKey': [true], 'altKey': [true]}
    }
  },
  {
    'command': 'nextRow',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [40], 'ctrlKey': [true], 'altKey': [true]}
    }
  },
  {
    'command': 'nextCol',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [39], 'ctrlKey': [true], 'altKey': [true]}
    }
  },
  {
    'command': 'previousCol',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [37], 'ctrlKey': [true], 'altKey': [true]}
    }
  },
  {
    'command': 'goToRowFirstCell',
    'sequence': {
      'cvoxModifier': true,
      'keys': {
        'keyCode': [37],
        'ctrlKey': [true],
        'altKey': [true],
        'shiftKey': [true]
      }
    }
  },
  {
    'command': 'goToColFirstCell',
    'sequence': {
      'cvoxModifier': true,
      'keys': {
        'keyCode': [38],
        'ctrlKey': [true],
        'altKey': [true],
        'shiftKey': [true]
      }
    }
  },
  {
    'command': 'goToColLastCell',
    'sequence': {
      'cvoxModifier': true,
      'keys': {
        'keyCode': [40],
        'ctrlKey': [true],
        'altKey': [true],
        'shiftKey': [true]
      }
    }
  },
  {
    'command': 'goToFirstCell',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [37], 'altKey': [true], 'shiftKey': [true]}
    }
  },
  {
    'command': 'goToLastCell',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [39], 'altKey': [true], 'shiftKey': [true]}
    }
  },
  {
    'command': 'goToRowLastCell',
    'sequence': {
      'cvoxModifier': true,
      'keys': {
        'keyCode': [39],
        'ctrlKey': [true],
        'altKey': [true],
        'shiftKey': [true]
      }
    }
  },
  {
    'command': 'previousGroup',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [38], 'ctrlKey': [true]}}
  },
  {
    'command': 'nextGroup',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [40], 'ctrlKey': [true]}}
  },
  {
    'command': 'previousSimilarItem',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [73], 'shiftKey': [true]}}
  },
  {
    'command': 'nextSimilarItem',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [73]}}
  },
  {
    'command': 'jumpToDetails',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 74]}}
  },
  {
    'command': 'toggleDarkScreen',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [217]}}
  },
  {
    'command': 'toggleSpeechOnOrOff',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [173]}}
  },
  {
    'command': 'enableChromeVoxArcSupportForCurrentApp',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 219]}}
  },
  {
    'command': 'disableChromeVoxArcSupportForCurrentApp',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 221]}}
  },
  {
    'command': 'forceClickOnCurrentItem',
    'sequence':
        {'cvoxModifier': true, 'keys': {'keyCode': [32]}, 'doubleTap': true}
  },
  {
    'command': 'showTtsSettings',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 83]}}
  },
  {
    'command': 'announceBatteryDescription',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [79, 66]}}
  },
  {
    'command': 'announceRichTextDescription',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 70]}}
  },
  {
    'command': 'readPhoneticPronunciation',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 67]}}
  },
  {
    'command': 'readLinkURL',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 76]}}
  },
  {
    'command': 'nextList',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [74, 76]}}
  },
  {
    'command': 'previousList',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [74, 76], 'shiftKey': [true]}
    }
  },
  {
    'command': 'resetTextToSpeechSettings',
    'sequence': {
      'cvoxModifier': true,
      'keys': {'keyCode': [220], 'ctrlKey': [true], 'shiftKey': [true]}
    }
  },
  {
    'command': 'toggleAnnotationsWidget',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [65, 79]}}
  },
  {
    'command': 'logLanguageInformationForCurrentNode',
    'sequence': {'cvoxModifier': true, 'keys': {'keyCode': [80, 76]}}
  }
];
