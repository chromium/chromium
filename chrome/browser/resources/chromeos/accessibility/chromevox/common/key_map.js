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
 * Due to execution of user commands within the content script, the function
 * name of the command is not explicitly checked within the background page via
 * Closure. Any errors would only be caught at runtime.
 *
 * To retrieve static data about user commands, see both CommandStore and
 * UserCommands.
 */
import {KeyCode} from '../../common/key_code.js';

import {Command} from './command_store.js';
import {KeySequence} from './key_sequence.js';

export class KeyMap {
  /**
   * @param {Array<Object<{command: !Command, sequence: KeySequence}>>}
   * commandsAndKeySequences An array of pairs - KeySequences and commands.
   */
  constructor(commandsAndKeySequences) {
    /**
     * An array of bindings - commands and KeySequences.
     * @type {Array<Object<{command: !Command, sequence: KeySequence}>>}
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
    return this.bindings_.map(binding => binding.sequence);
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
   * @param {!Command} command The command.
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
   * @param {!Command} command The command to check.
   * @return {boolean} Whether |command| has a binding.
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
   * @return {?Command} The command, if any.
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
   * @param {!Command} command The command to query.
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
         * @type {Array<Object<{command: !Command,
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
}

// This is intentionally not type-checked, as it is a serialized set of
// KeySequence objects.
/** @private {!Object} */
KeyMap.BINDINGS_ = [
  {
    command: Command.PREVIOUS_OBJECT,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT]}},
  },
  {
    command: Command.PREVIOUS_LINE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.UP]}},
  },
  {
    command: Command.NEXT_OBJECT,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT]}},
  },
  {
    command: Command.NEXT_LINE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN]}},
  },
  {
    command: Command.NEXT_CHARACTER,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], shiftKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_CHARACTER,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], shiftKey: [true]}},
  },
  {
    command: Command.NATIVE_NEXT_CHARACTER,
    sequence: {cvoxModifier: false, keys: {keyCode: [KeyCode.RIGHT]}},
  },
  {
    command: Command.NATIVE_PREVIOUS_CHARACTER,
    sequence: {cvoxModifier: false, keys: {keyCode: [KeyCode.LEFT]}},
  },
  {
    command: Command.NEXT_WORD,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_WORD,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.NATIVE_NEXT_WORD,
    sequence: {
      cvoxModifier: false,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true]},
    },
  },
  {
    command: Command.NATIVE_PREVIOUS_WORD,
    sequence:
        {cvoxModifier: false, keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true]}},
  },
  {
    command: Command.NEXT_BUTTON,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.B]}},
  },
  {
    command: Command.PREVIOUS_BUTTON,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.B], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_CHECKBOX,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.X]}},
  },
  {
    command: Command.PREVIOUS_CHECKBOX,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.X], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_COMBO_BOX,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.C]}},
  },
  {
    command: Command.PREVIOUS_COMBO_BOX,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_EDIT_TEXT,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.E]}},
  },
  {
    command: Command.PREVIOUS_EDIT_TEXT,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.E], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_FORM_FIELD,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.F]}},
  },
  {
    command: Command.PREVIOUS_FORM_FIELD,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_GRAPHIC,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.G], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_GRAPHIC,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.G]}},
  },
  {
    command: Command.NEXT_HEADING,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.H]}},
  },
  {
    command: Command.NEXT_HEADING_1,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE]}},
  },
  {
    command: Command.NEXT_HEADING_2,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO]}},
  },
  {
    command: Command.NEXT_HEADING_3,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.THREE]}},
  },
  {
    command: Command.NEXT_HEADING_4,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR]}},
  },
  {
    command: Command.NEXT_HEADING_5,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE]}},
  },
  {
    command: Command.NEXT_HEADING_6,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX]}},
  },
  {
    command: Command.PREVIOUS_HEADING,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_1,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_2,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_3,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.THREE], shiftKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_HEADING_4,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_5,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_6,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_LINK,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.L]}},
  },
  {
    command: Command.PREVIOUS_LINK,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_TABLE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.T]}},
  },
  {
    command: Command.PREVIOUS_TABLE,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_VISITED_LINK,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.V]}},
  },
  {
    command: Command.PREVIOUS_VISITED_LINK,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.V], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_LANDMARK,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1]}},
  },
  {
    command: Command.PREVIOUS_LANDMARK,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_1], shiftKey: [true]},
    },
  },
  {
    command: Command.JUMP_TO_BOTTOM,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true]}},
  },
  {
    command: Command.JUMP_TO_TOP,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true]}},
  },
  {
    command: Command.FORCE_CLICK_ON_CURRENT_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}},
  },
  {
    command: Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.SPACE], shiftKey: [true]},
    },
  },
  {
    command: Command.CONTEXT_MENU,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.M]}},
  },
  {
    command: Command.READ_FROM_HERE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.R]}},
  },
  {
    command: Command.TOGGLE_STICKY_MODE,
    sequence: {
      skipStripping: false,
      doubleTap: true,
      keys: {keyCode: [KeyCode.SEARCH]},
    },
  },
  {
    command: Command.PASS_THROUGH_MODE,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.ESCAPE], shiftKey: [true]},
    },
  },
  {
    command: Command.TOGGLE_KEYBOARD_HELP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_PERIOD]}},
  },
  {
    command: Command.STOP_SPEECH,
    sequence: {
      cvoxModifier: false,
      keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]},
    },
  },
  {
    command: Command.DECREASE_TTS_RATE,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_4], shiftKey: [true]},
    },
  },
  {
    command: Command.INCREASE_TTS_RATE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_4]}},
  },
  {
    command: Command.DECREASE_TTS_PITCH,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_6], shiftKey: [true]},
    },
  },
  {
    command: Command.INCREASE_TTS_PITCH,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_6]}},
  },
  {
    command: Command.STOP_SPEECH,
    sequence: {keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]}},
  },
  {
    command: Command.CYCLE_PUNCTUATION_ECHO,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.P]}},
  },
  {
    command: Command.SHOW_LEARN_MODE_PAGE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.K]}},
  },
  {
    command: Command.CYCLE_TYPING_ECHO,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.T]}},
  },
  {
    command: Command.SHOW_OPTIONS_PAGE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.O]}},
  },
  {
    command: Command.SHOW_LOG_PAGE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.W]}},
  },
  {
    command: Command.ENABLE_LOGGING,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.E]}},
  },
  {
    command: Command.DISABLE_LOGGING,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.D]}},
  },
  {
    command: Command.DUMP_TREE,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.D, KeyCode.T], ctrlKey: [true]},
    },
  },
  {
    command: Command.HELP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.T]}},
  },
  {
    command: Command.TOGGLE_EARCONS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.E]}},
  },
  {
    command: Command.SPEAK_TIME_AND_DATE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.D]}},
  },
  {
    command: Command.READ_CURRENT_TITLE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.W]}},
  },
  {
    command: Command.READ_CURRENT_URL,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.U]}},
  },
  {
    command: Command.REPORT_ISSUE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.I]}},
  },
  {
    command: Command.TOGGLE_SEARCH_WIDGET,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_2]}},
  },
  {
    command: Command.SHOW_HEADINGS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_FORMS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_LANDMARKS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_LINKS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_ACTIONS_MENU,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.A], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_TABLES_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], ctrlKey: [true]}},
  },
  {
    command: Command.TOGGLE_BRAILLE_CAPTIONS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.B]}},
  },
  {
    command: Command.TOGGLE_BRAILLE_TABLE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.G]}},
  },
  {
    command: Command.VIEW_GRAPHIC_AS_BRAILLE,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.G], altKey: [true]}},
  },
  {
    command: Command.TOGGLE_SELECTION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.S]}},
  },
  {
    command: Command.FULLY_DESCRIBE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.K]}},
  },
  {
    command: Command.PREVIOUS_ROW,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.UP], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.NEXT_ROW,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.NEXT_COL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_COL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.GO_TO_ROW_FIRST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.LEFT],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.GO_TO_COL_FIRST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.UP],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.GO_TO_COL_LAST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.DOWN],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.GO_TO_FIRST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], altKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.GO_TO_LAST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], altKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.GO_TO_ROW_LAST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.RIGHT],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.PREVIOUS_GROUP,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.UP], ctrlKey: [true]}},
  },
  {
    command: Command.NEXT_GROUP,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true]}},
  },
  {
    command: Command.PREVIOUS_SIMILAR_ITEM,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.I], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_SIMILAR_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.I]}},
  },
  {
    command: Command.PREVIOUS_INVALID_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.P, KeyCode.I]}},
  },
  {
    command: Command.NEXT_INVALID_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.N, KeyCode.I]}},
  },
  {
    command: Command.JUMP_TO_DETAILS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.J]}},
  },
  {
    command: Command.TOGGLE_SCREEN,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.BRIGHTNESS_UP]}},
  },
  {
    command: Command.TOGGLE_SPEECH_ON_OR_OFF,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.VOLUME_MUTE]}},
  },
  {
    command: Command.ENABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_4]}},
  },
  {
    command: Command.DISABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_6]}},
  },
  {
    command: Command.SHOW_TALKBACK_KEYBOARD_SHORTCUTS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.K]}},
  },
  {
    command: Command.FORCE_CLICK_ON_CURRENT_ITEM,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}, doubleTap: true},
  },
  {
    command: Command.SHOW_TTS_SETTINGS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.S]}},
  },
  {
    command: Command.ANNOUNCE_BATTERY_DESCRIPTION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.B]}},
  },
  {
    command: Command.ANNOUNCE_RICH_TEXT_DESCRIPTION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.F]}},
  },
  {
    command: Command.READ_PHONETIC_PRONUNCIATION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.C]}},
  },
  {
    command: Command.READ_LINK_URL,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.L]}},
  },
  {
    command: Command.NEXT_LIST,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.J, KeyCode.L]}},
  },
  {
    command: Command.PREVIOUS_LIST,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.J, KeyCode.L], shiftKey: [true]},
    },
  },
  {
    command: Command.RESET_TEXT_TO_SPEECH_SETTINGS,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_5], ctrlKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.COPY,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], ctrlKey: [true]}},
  },
  {
    command: Command.TOGGLE_DICTATION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.D]}},
  },
];
