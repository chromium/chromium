// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   commandString: string,
 *   matchesCommand: function(string):boolean,
 *   matchesTypeCommand: function(string):boolean,
 * }}
 */
let ActionInfo;

/**
 * CommandParser handles parsing text editing commands.
 */
export class CommandParser {
  constructor() {
    /** @private {boolean} */
    this.commandsFeatureEnabled_ = false;

    /**
     * Map of command action IDs to information about that command.
     * This object uses localized strings.
     * @private {!Map<Command.Action, ActionInfo>}
     */
    this.commandMap_ = new Map();

    chrome.accessibilityPrivate.isFeatureEnabled(
        chrome.accessibilityPrivate.AccessibilityFeature.DICTATION_COMMANDS,
        (result) => {
          this.commandsFeatureEnabled_ = result;
          if (this.commandsFeatureEnabled_) {
            this.initializeCommandMap_();
          }
        });
  }

  /**
   * Parses user text to produce a command.
   * @param {string} text The text to parse into a command.
   * @return {Command}
   */
  parse(text) {
    if (!this.commandsFeatureEnabled_) {
      // Without ExperimentalAccessibilityDictationCommands feature, all
      // text should be input as-is.
      return new Command(Command.Action.INPUT_TEXT, text);
    }

    // See if the text matches a command action.
    for (const [action, info] of this.commandMap_) {
      if (info.matchesCommand(text)) {
        return new Command(/** @type {Command.Action} */ (action));
      } else if (info.matchesTypeCommand(text)) {
        text = info.commandString;
      }
    }
    // The command is simply to input the given text.
    return new Command(Command.Action.INPUT_TEXT, text);
  }

  /**
   * Pre-work to parse commands. Gets translated command strings and generates
   * a map of commands to regular expressions that would match them.
   * @private
   */
  initializeCommandMap_() {
    for (const key in Command.Action) {
      const actionId = Command.Action[key];
      let messageId;
      switch (actionId) {
        case Command.Action.DELETE_ONCE:
          messageId = 'dictation_command_delete_once';
          break;
        case Command.Action.MOVE_LEFT_ONCE:
          messageId = 'dictation_command_move_left_once';
          break;
        case Command.Action.MOVE_RIGHT_ONCE:
          messageId = 'dictation_command_move_right_once';
          break;
        case Command.Action.COPY:
          // TODO(1247299): This command requires text to be selected but
          // composition text changes during speech, clearing the selection.
          // This will work better when we use a different UI than composition
          // text to display interim results.
          messageId = 'dictation_command_copy';
          break;
        case Command.Action.PASTE:
          messageId = 'dictation_command_paste';
          break;
        case Command.Action.CUT:
          messageId = 'dictation_command_cut';
          break;
        case Command.Action.UNDO:
          messageId = 'dictation_command_undo';
          break;
        case Command.Action.REDO:
          messageId = 'dictation_command_redo';
          break;
        default:
          continue;
      }
      const commandString = chrome.i18n.getMessage(messageId);
      this.commandMap_.set(actionId, {
        commandString,
        matchesCommand: this.commandMatcher_(commandString),
        matchesTypeCommand: this.typeCommandMatcher_(commandString)
      });
    }
  }

  /**
   * Gets a function that checks whether a string matches a
   * command string, ignoring case and whitespace.
   * @param {string} commandString
   * @return {function(string):boolean}
   * @private
   */
  commandMatcher_(commandString) {
    const re = new RegExp('^[\\s]*' + commandString + '[\\s]*$', 'im');
    return text => {
      return re.exec(text);
    };
  }

  /**
   * Gets a function that checks whether a string matches
   * a request to type a command, i.e. for the command 'delete', it would
   * match 'type delete', ignoring case and whitespace.
   * @param {string} commandString
   * @return {function(string):boolean}
   * @private
   */
  typeCommandMatcher_(commandString) {
    const query =
        chrome.i18n.getMessage('dictation_command_type_text', commandString);
    const re = new RegExp('^[\\s]*' + query + '[\\s]*$', 'im');
    return text => {
      return re.exec(text);
    };
  }
}

/**
 * Command represents a text editing command.
 */
export class Command {
  /**
   * Constructs a new command for the particular action and given text (if
   * applicable).
   * @param {Command.Action} action
   * @param {string=} text
   */
  constructor(action, text = '') {
    /** @private {string} */
    this.text_ = text;

    /** @private {Command.Action} */
    this.action_ = action;
  }

  /**
   * @return {boolean} Whether this command is a request to input text.
   */
  isTextInput() {
    return this.action_ === Command.Action.INPUT_TEXT;
  }

  /**
   * The text of the command, if any.
   * @return {string} The text to input.
   */
  getText() {
    return this.text_;
  }

  /**
   * Executes the command.
   */
  execute() {
    // Commands using keyboard shortcuts.
    switch (this.action_) {
      case Command.Action.INPUT_TEXT:
        // Text input is not handled here.
        break;
      case Command.Action.DELETE_ONCE:
        EventGenerator.sendKeyPress(KeyCode.BACK);
        break;
      case Command.Action.MOVE_LEFT_ONCE:
        EventGenerator.sendKeyPress(KeyCode.LEFT);
        break;
      case Command.Action.MOVE_RIGHT_ONCE:
        EventGenerator.sendKeyPress(KeyCode.RIGHT);
        break;
      case Command.Action.COPY:
        EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});
        break;
      case Command.Action.PASTE:
        EventGenerator.sendKeyPress(KeyCode.V, {ctrl: true});
        break;
      case Command.Action.CUT:
        EventGenerator.sendKeyPress(KeyCode.X, {ctrl: true});
        break;
      case Command.Action.UNDO:
        EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true});
        break;
      case Command.Action.REDO:
        EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true, shift: true});
        break;
      default:
        console.warn(
            'Cannot execute action ' +
            Object.keys(Command.Action)
                .find(key => Command.Action[key] === this.action_));
    }
  }
}

/**
 * Possible command action types.
 * @enum {number}
 * @private
 */
Command.Action = {
  // Simply input text into a text field.
  INPUT_TEXT: 1,

  // Delete one character.
  DELETE_ONCE: 2,

  // Move left one character.
  MOVE_LEFT_ONCE: 3,

  // Move right one character.
  MOVE_RIGHT_ONCE: 4,

  // Copy any selected text, using clipboard copy.
  COPY: 5,

  // Paste any clipboard text.
  PASTE: 6,

  // Cut (copy and delete) any selected text.
  CUT: 7,

  // Undo previous text-editing action. Does not undo
  // previous navigation or selection action, does not
  // clear clipboard.
  UNDO: 8,

  // Redo previous text-editing action. Does not redo
  // previous navigation or selection action, does not
  // clear clipboard.
  REDO: 9,
};
