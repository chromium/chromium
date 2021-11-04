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

    /** @private {boolean} */
    this.isRTLLocale_ = false;
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
      return new Command(
          Command.Action.INPUT_TEXT_VIEW, this.isRTLLocale_, text);
    }

    // See if the text matches a command action.
    for (const [action, info] of this.commandMap_) {
      if (info.matchesCommand(text)) {
        return new Command(
            /** @type {Command.Action} */ (action), this.isRTLLocale_);
      } else if (info.matchesTypeCommand(text)) {
        text = info.commandString;
      }
    }
    // The command is simply to input the given text.
    return new Command(Command.Action.INPUT_TEXT_VIEW, this.isRTLLocale_, text);
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
   * @param {string} locale The Dictation recognition locale.
   */
  setCommandsEnabled(locale) {
    this.isRTLLocale_ = CommandParser.RTLLocales.has(locale);
    this.commandsFeatureEnabled_ = true;
    for (const key in Command.Action) {
      const actionId = Command.Action[key];
      let messageId;
      switch (actionId) {
        case Command.Action.DELETE_PREV_CHAR:
          messageId = 'dictation_command_delete_prev_char';
          break;
        case Command.Action.NAV_PREV_CHAR:
          messageId = 'dictation_command_nav_prev_char';
          break;
        case Command.Action.NAV_NEXT_CHAR:
          messageId = 'dictation_command_nav_next_char';
          break;
        case Command.Action.NAV_PREV_LINE:
          messageId = 'dictation_command_nav_prev_line';
          break;
        case Command.Action.NAV_NEXT_LINE:
          messageId = 'dictation_command_nav_next_line';
          break;
        case Command.Action.COPY_SELECTED_TEXT:
          messageId = 'dictation_command_copy_selected_text';
          break;
        case Command.Action.PASTE_TEXT:
          messageId = 'dictation_command_paste_text';
          break;
        case Command.Action.CUT_SELECTED_TEXT:
          messageId = 'dictation_command_cut_selected_text';
          break;
        case Command.Action.UNDO_TEXT_EDIT:
          messageId = 'dictation_command_undo_text_edit';
          break;
        case Command.Action.REDO_ACTION:
          messageId = 'dictation_command_redo_action';
          break;
        case Command.Action.SELECT_ALL_TEXT:
          messageId = 'dictation_command_select_all_text';
          break;
        case Command.Action.UNSELECT_TEXT:
          messageId = 'dictation_command_unselect_text';
          break;
        case Command.Action.LIST_COMMANDS:
          messageId = 'dictation_command_list_commands';
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
    const query = chrome.i18n.getMessage(
        'dictation_command_input_text_view', commandString);
    const re = new RegExp('^[\\s]*' + query + '[\\s]*$', 'im');
    return text => {
      return re.exec(text);
    };
  }
}

// All RTL locales from Dictation::GetAllSupportedLocales.
CommandParser.RTLLocales = new Set([
  'ar-AE', 'ar-BH', 'ar-DZ', 'ar-EG', 'ar-IL', 'ar-IQ', 'ar-JO',
  'ar-KW', 'ar-LB', 'ar-MA', 'ar-OM', 'ar-PS', 'ar-QA', 'ar-SA',
  'ar-TN', 'ar-YE', 'fa-IR', 'iw-IL', 'ur-IN', 'ur-PK'
]);

/**
 * Command represents a text editing command.
 */
export class Command {
  /**
   * Constructs a new command for the particular action and given text (if
   * applicable).
   * @param {Command.Action} action
   * @param {boolean} isRTLLocale
   * @param {string=} text
   */
  constructor(action, isRTLLocale, text = '') {
    /** @private {string} */
    this.text_ = text;

    /** @private {Command.Action} */
    this.action_ = action;

    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;
  }

  /**
   * @return {boolean} Whether this command is a request to input text.
   */
  isTextInput() {
    return this.action_ === Command.Action.INPUT_TEXT_VIEW ||
        this.action_ === Command.Action.NEW_LINE;
  }

  /**
   * @return {boolean} Whether this command ends Dictation. If so, we shouldn't
   * try to show additional annotations or interact with the IME.
   * TODO(crbug.com/1252037): This can probably be removed when the true
   * Dictation UI is used instead of IME UI as an error occurs when showing the
   * IME annotation.
   */
  endsDictation() {
    return this.action_ === Command.Action.LIST_COMMANDS;
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
      case Command.Action.INPUT_TEXT_VIEW:
      case Command.Action.NEW_LINE:
        // Text input is not handled here.
        break;
      case Command.Action.DELETE_PREV_CHAR:
        EventGenerator.sendKeyPress(KeyCode.BACK);
        break;
      case Command.Action.NAV_PREV_CHAR:
        EventGenerator.sendKeyPress(
            this.isRTLLocale_ ? KeyCode.RIGHT : KeyCode.LEFT);
        break;
      case Command.Action.NAV_NEXT_CHAR:
        EventGenerator.sendKeyPress(
            this.isRTLLocale_ ? KeyCode.LEFT : KeyCode.RIGHT);
        break;
      case Command.Action.NAV_PREV_LINE:
        EventGenerator.sendKeyPress(KeyCode.UP);
        break;
      case Command.Action.NAV_NEXT_LINE:
        EventGenerator.sendKeyPress(KeyCode.DOWN);
        break;
      case Command.Action.COPY_SELECTED_TEXT:
        EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});
        break;
      case Command.Action.PASTE_TEXT:
        EventGenerator.sendKeyPress(KeyCode.V, {ctrl: true});
        break;
      case Command.Action.CUT_SELECTED_TEXT:
        EventGenerator.sendKeyPress(KeyCode.X, {ctrl: true});
        break;
      case Command.Action.UNDO_TEXT_EDIT:
        EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true});
        break;
      case Command.Action.REDO_ACTION:
        EventGenerator.sendKeyPress(KeyCode.Z, {ctrl: true, shift: true});
        break;
      case Command.Action.SELECT_ALL_TEXT:
        EventGenerator.sendKeyPress(KeyCode.A, {ctrl: true});
        break;
      case Command.Action.UNSELECT_TEXT:
        // TODO(crbug.com/1247299): Internationalization: might want to move
        // left in RTL application languages, or restore previous caret
        // position from before selection began, if available. Use the
        // Dictation locale to decide if a locale is RTL or LTR.
        EventGenerator.sendKeyPress(KeyCode.RIGHT);
        break;
      case Command.Action.LIST_COMMANDS:
        // Note that this will open a new tab, probably ending the current
        // Dictation by changing the input focus.
        // TODO(crbug.com/1247299): This support page does not exist. Make sure
        // to get the correct URL before launch.
        window.open(
            'https://support.google.com/chromebook?p=dictation', '_blank');
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
  INPUT_TEXT_VIEW: 1,

  // Delete one character.
  DELETE_PREV_CHAR: 2,

  // Move the cursor to the previous character.
  NAV_PREV_CHAR: 3,

  // Move the cursor to the next character.
  NAV_NEXT_CHAR: 4,

  // Move up to the previous line.
  NAV_PREV_LINE: 5,

  // Move down to the next line.
  NAV_NEXT_LINE: 6,

  // Copy any selected text, using clipboard copy.
  COPY_SELECTED_TEXT: 7,

  // Paste any clipboard text.
  PASTE_TEXT: 8,

  // Cut (copy and delete) any selected text.
  CUT_SELECTED_TEXT: 9,

  // Undo previous text-editing action. Does not undo
  // previous navigation or selection action, does not
  // clear clipboard.
  UNDO_TEXT_EDIT: 10,

  // Redo previous text-editing action. Does not redo
  // previous navigation or selection action, does not
  // clear clipboard.
  REDO_ACTION: 11,

  // Select all text in the text field.
  SELECT_ALL_TEXT: 12,

  // Clears the current selection, moving the cursor to
  // the end of the selection.
  UNSELECT_TEXT: 13,

  // Lists available Dictation commands by bringing up the Help page.
  LIST_COMMANDS: 14,

  // Insert a new line character.
  // Note: This doesn't correspond to a Voice Access action.s
  NEW_LINE: 15,
};
