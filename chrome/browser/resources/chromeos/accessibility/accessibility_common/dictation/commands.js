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
   * Gets the description of a command.
   * @param {Command} command
   * @return {string}
   */
  getCommandString(command) {
    if (command.isTextInput()) {
      return '';
    }
    return this.commandMap_.get(command.action_).commandString;
  }

  /**
   * Enables commands. Does pre-work to parse commands: gets translated command
   * strings and generates a map of commands to regular expressions that would
   * match them.
   */
  setCommandsEnabled() {
    this.commandsFeatureEnabled_ = true;
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
        case Command.Action.MOVE_UP_ONCE:
          messageId = 'dictation_command_move_up_once';
          break;
        case Command.Action.MOVE_DOWN_ONCE:
          messageId = 'dictation_command_move_down_once';
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
        case Command.Action.SELECT_ALL:
          messageId = 'dictation_command_select_all';
          break;
        case Command.Action.UNSELECT_ALL:
          messageId = 'dictation_command_unselect_all';
          break;
        case Command.Action.NEW_LINE:
          messageId = 'dictation_command_new_line';
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
    return this.action_ === Command.Action.INPUT_TEXT ||
        this.action_ === Command.Action.NEW_LINE;
  }

  /**
   * The text of the command, if any.
   * @return {string} The text to input.
   */
  getText() {
    if (this.action_ === Command.Action.NEW_LINE) {
      return '\n';
    }
    return this.text_;
  }

  /**
   * Executes the command.
   * @return {boolean} Whether execution was successful.
   */
  execute() {
    // Commands using keyboard shortcuts.
    switch (this.action_) {
      case Command.Action.INPUT_TEXT:
      case Command.Action.NEW_LINE:
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
      case Command.Action.MOVE_UP_ONCE:
        EventGenerator.sendKeyPress(KeyCode.UP);
        break;
      case Command.Action.MOVE_DOWN_ONCE:
        EventGenerator.sendKeyPress(KeyCode.DOWN);
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
      case Command.Action.SELECT_ALL:
        EventGenerator.sendKeyPress(KeyCode.A, {ctrl: true});
        break;
      case Command.Action.UNSELECT_ALL:
        // TODO(crbug.com/1247299): Internationalization: might want to move
        // left in RTL application languages, or restore previous caret position
        // from before selection began, if available.
        EventGenerator.sendKeyPress(KeyCode.RIGHT);
        break;
      default:
        console.warn(
            'Cannot execute action ' +
            Object.keys(Command.Action)
                .find(key => Command.Action[key] === this.action_));
        return false;
    }
    return true;
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

  // Move up one line.
  MOVE_UP_ONCE: 5,

  // Move down one line.
  MOVE_DOWN_ONCE: 6,

  // Copy any selected text, using clipboard copy.
  COPY: 7,

  // Paste any clipboard text.
  PASTE: 8,

  // Cut (copy and delete) any selected text.
  CUT: 9,

  // Undo previous text-editing action. Does not undo
  // previous navigation or selection action, does not
  // clear clipboard.
  UNDO: 10,

  // Redo previous text-editing action. Does not redo
  // previous navigation or selection action, does not
  // clear clipboard.
  REDO: 11,

  // Select all text in the text field.
  SELECT_ALL: 12,

  // Clears the current selection, moving the cursor to
  // the end of the selection.
  UNSELECT_ALL: 13,

  // Insert a new line character.
  NEW_LINE: 14,
};
