// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This class acts as the persistent store for all static data
 * about commands.
 *
 * This store can safely be used within either a content or background script
 * context.
 *
 * If you are looking to add a user command, follow the below steps for best
 * integration with existing components:
 * 1. Add the command to the |Command| enum.
 * 2. Add a command below in CommandStore.COMMAND_DATA. Fill in each of the
 * relevant JSON keys.
 * Be sure to add a msg id and define it in chromevox/messages/messages.js which
 * describes the command. Please also add a category msg id so that the command
 * will show up in the options page.
 * 2. Add the command's logic to CommandHandler inside of our switch-based
 * dispatch method (onCommand).
 * 3. Add a key binding to KeySequence.
 */

export class CommandStore {
  /**
   * Gets a message given a command.
   * @param {!Command} command The command to query.
   * @return {string|undefined} The message id, if any.
   */
  static messageForCommand(command) {
    return (CommandStore.COMMAND_DATA[command] || {}).msgId;
  }

  /**
   * Gets a category given a command.
   * @param {!Command} command The command to query.
   * @return {string|undefined} The category, if any.
   */
  static categoryForCommand(command) {
    return (CommandStore.COMMAND_DATA[command] || {}).category;
  }

  /**
   * Gets the first command associated with the message id
   * @param {string} msgId
   * @return {!Command|undefined} The command, if any.
   */
  static commandForMessage(msgId) {
    for (const commandName in CommandStore.COMMAND_DATA) {
      const command = CommandStore.COMMAND_DATA[commandName];
      if (command.msgId === msgId) {
        return commandName;
      }
    }
  }

  /**
   * Gets all commands for a category.
   * @param {string} category The category to query.
   * @return {Array<!Command>} The commands, if any.
   */
  static commandsForCategory(category) {
    const ret = [];
    for (const cmd in CommandStore.COMMAND_DATA) {
      const struct = CommandStore.COMMAND_DATA[cmd];
      if (category === struct.category) {
        ret.push(cmd);
      }
    }
    return ret;
  }

  /**
   * @param {!Command} command The command to query.
   * @return {boolean} Whether or not this command is denied in the OOBE.
   */
  static denySignedOut(command) {
    if (!CommandStore.COMMAND_DATA[command]) {
      return false;
    }
    return Boolean(CommandStore.COMMAND_DATA[command].denySignedOut);
  }
}

/**
 * List of commands. Please keep list alphabetical.
 * @enum {string}
 */
export const Command = {
  ANNOUNCE_BATTERY_DESCRIPTION: 'announceBatteryDescription',
  ANNOUNCE_HEADERS: 'announceHeaders',
  ANNOUNCE_RICH_TEXT_DESCRIPTION: 'announceRichTextDescription',
  AUTORUNNER: 'autorunner',
  BACKWARD: 'backward',
  BOTTOM: 'bottom',
  CONTEXT_MENU: 'contextMenu',
  COPY: 'copy',
  CYCLE_PUNCTUATION_ECHO: 'cyclePunctuationEcho',
  CYCLE_TYPING_ECHO: 'cycleTypingEcho',
  DEBUG: 'debug',
  DECREASE_TTS_PITCH: 'decreaseTtsPitch',
  DECREASE_TTS_RATE: 'decreaseTtsRate',
  DECREASE_TTS_VOLUME: 'decreaseTtsVolume',
  DISABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP:
      'disableChromeVoxArcSupportForCurrentApp',
  DISABLE_LOGGING: 'disableLogging',
  DUMP_TREE: 'dumpTree',
  ENABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP:
      'enableChromeVoxArcSupportForCurrentApp',
  ENABLE_CONSOLE_TTS: 'enableConsoleTts',
  ENABLE_LOGGING: 'enableLogging',
  ENTER_SHIFTER: 'enterShifter',
  EXIT_SHIFTER: 'exitShifter',
  EXIT_SHIFTER_CONTENT: 'exitShifterContent',
  FORCE_CLICK_ON_CURRENT_ITEM: 'forceClickOnCurrentItem',
  FORCE_DOUBLE_CLICK_ON_CURRENT_ITEM: 'forceDoubleClickOnCurrentItem',
  FORCE_LONG_CLICK_ON_CURRENT_ITEM: 'forceLongClickOnCurrentItem',
  FORWARD: 'forward',
  FULLY_DESCRIBE: 'fullyDescribe',
  GO_TO_COL_FIRST_CELL: 'goToColFirstCell',
  GO_TO_COL_LAST_CELL: 'goToColLastCell',
  GO_TO_FIRST_CELL: 'goToFirstCell',
  GO_TO_LAST_CELL: 'goToLastCell',
  GO_TO_ROW_FIRST_CELL: 'goToRowFirstCell',
  GO_TO_ROW_LAST_CELL: 'goToRowLastCell',
  HANDLE_TAB: 'handleTab',
  HANDLE_TAB_PREV: 'handleTabPrev',
  HELP: 'help',
  INCREASE_TTS_PITCH: 'increaseTtsPitch',
  INCREASE_TTS_RATE: 'increaseTtsRate',
  INCREASE_TTS_VOLUME: 'increaseTtsVolume',
  JUMP_TO_BOTTOM: 'jumpToBottom',
  JUMP_TO_DETAILS: 'jumpToDetails',
  JUMP_TO_TOP: 'jumpToTop',
  LEFT: 'left',
  LINE_DOWN: 'lineDown',
  LINE_UP: 'lineUp',
  MOVE_TO_END_OF_LINE: 'moveToEndOfLine',
  MOVE_TO_START_OF_LINE: 'moveToStartOfLine',
  NOP: 'nop',
  NATIVE_NEXT_CHARACTER: 'nativeNextCharacter',
  NATIVE_NEXT_WORD: 'nativeNextWord',
  NATIVE_PREVIOUS_CHARACTER: 'nativePreviousCharacter',
  NATIVE_PREVIOUS_WORD: 'nativePreviousWord',
  NEXT_ARTICLE: 'nextArticle',
  NEXT_AT_GRANULARITY: 'nextAtGranularity',
  NEXT_BUTTON: 'nextButton',
  NEXT_CHARACTER: 'nextCharacter',
  NEXT_CHECKBOX: 'nextCheckbox',
  NEXT_COL: 'nextCol',
  NEXT_COMBO_BOX: 'nextComboBox',
  NEXT_CONTROL: 'nextControl',
  NEXT_EDIT_TEXT: 'nextEditText',
  NEXT_FORM_FIELD: 'nextFormField',
  NEXT_GRANULARITY: 'nextGranularity',
  NEXT_GRAPHIC: 'nextGraphic',
  NEXT_GROUP: 'nextGroup',
  NEXT_HEADING: 'nextHeading',
  NEXT_HEADING_1: 'nextHeading1',
  NEXT_HEADING_2: 'nextHeading2',
  NEXT_HEADING_3: 'nextHeading3',
  NEXT_HEADING_4: 'nextHeading4',
  NEXT_HEADING_5: 'nextHeading5',
  NEXT_HEADING_6: 'nextHeading6',
  NEXT_INVALID_ITEM: 'nextInvalidItem',
  NEXT_LANDMARK: 'nextLandmark',
  NEXT_LINE: 'nextLine',
  NEXT_LINK: 'nextLink',
  NEXT_LIST: 'nextList',
  NEXT_LIST_ITEM: 'nextListItem',
  NEXT_MATH: 'nextMath',
  NEXT_MEDIA: 'nextMedia',
  NEXT_OBJECT: 'nextObject',
  NEXT_PAGE: 'nextPage',
  NEXT_RADIO: 'nextRadio',
  NEXT_ROW: 'nextRow',
  NEXT_SECTION: 'nextSection',
  NEXT_SENTENCE: 'nextSentence',
  NEXT_SIMILAR_ITEM: 'nextSimilarItem',
  NEXT_SLIDER: 'nextSlider',
  NEXT_TABLE: 'nextTable',
  NEXT_VISITED_LINK: 'nextVisitedLink',
  NEXT_WORD: 'nextWord',
  OPEN_CHROMEVOX_MENUS: 'openChromeVoxMenus',
  OPEN_LONG_DESC: 'openLongDesc',
  PAN_LEFT: 'panLeft',
  PAN_RIGHT: 'panRight',
  PASS_THROUGH_MODE: 'passThroughMode',
  PAUSE_ALL_MEDIA: 'pauseAllMedia',
  PREVIOUS_ARTICLE: 'previousArticle',
  PREVIOUS_AT_GRANULARITY: 'previousAtGranularity',
  PREVIOUS_BUTTON: 'previousButton',
  PREVIOUS_CHARACTER: 'previousCharacter',
  PREVIOUS_CHECKBOX: 'previousCheckbox',
  PREVIOUS_COMBO_BOX: 'previousComboBox',
  PREVIOUS_COL: 'previousCol',
  PREVIOUS_CONTROL: 'previousControl',
  PREVIOUS_EDIT_TEXT: 'previousEditText',
  PREVIOUS_FORM_FIELD: 'previousFormField',
  PREVIOUS_GRANULARITY: 'previousGranularity',
  PREVIOUS_GRAPHIC: 'previousGraphic',
  PREVIOUS_GROUP: 'previousGroup',
  PREVIOUS_HEADING: 'previousHeading',
  PREVIOUS_HEADING_1: 'previousHeading1',
  PREVIOUS_HEADING_2: 'previousHeading2',
  PREVIOUS_HEADING_3: 'previousHeading3',
  PREVIOUS_HEADING_4: 'previousHeading4',
  PREVIOUS_HEADING_5: 'previousHeading5',
  PREVIOUS_HEADING_6: 'previousHeading6',
  PREVIOUS_INVALID_ITEM: 'previousInvalidItem',
  PREVIOUS_LANDMARK: 'previousLandmark',
  PREVIOUS_LINE: 'previousLine',
  PREVIOUS_LINK: 'previousLink',
  PREVIOUS_LIST: 'previousList',
  PREVIOUS_LIST_ITEM: 'previousListItem',
  PREVIOUS_MATH: 'previousMath',
  PREVIOUS_MEDIA: 'previousMedia',
  PREVIOUS_OBJECT: 'previousObject',
  PREVIOUS_PAGE: 'previousPage',
  PREVIOUS_RADIO: 'previousRadio',
  PREVIOUS_ROW: 'previousRow',
  PREVIOUS_SECTION: 'previousSection',
  PREVIOUS_SENTENCE: 'previousSentence',
  PREVIOUS_SIMILAR_ITEM: 'previousSimilarItem',
  PREVIOUS_SLIDER: 'previousSlider',
  PREVIOUS_TABLE: 'previousTable',
  PREVIOUS_VISITED_LINK: 'previousVisitedLink',
  PREVIOUS_WORD: 'previousWord',
  READ_CURRENT_TITLE: 'readCurrentTitle',
  READ_CURRENT_URL: 'readCurrentURL',
  READ_FROM_HERE: 'readFromHere',
  READ_LINK_URL: 'readLinkURL',
  READ_PHONETIC_PRONUNCIATION: 'readPhoneticPronunciation',
  REPORT_ISSUE: 'reportIssue',
  RESET_TEXT_TO_SPEECH_SETTINGS: 'resetTextToSpeechSettings',
  RIGHT: 'right',
  ROUTING: 'routing',
  SCROLL_BACKWARD: 'scrollBackward',
  SCROLL_FORWARD: 'scrollForward',
  SHOW_ACTIONS_MENU: 'showActionsMenu',
  SHOW_FORMS_LIST: 'showFormsList',
  SHOW_HEADINGS_LIST: 'showHeadingsList',
  SHOW_LANDMARKS_LIST: 'showLandmarksList',
  SHOW_LEARN_MODE_PAGE: 'showLearnModePage',
  SHOW_LINKS_LIST: 'showLinksList',
  SHOW_LOG_PAGE: 'showLogPage',
  SHOW_OPTIONS_PAGE: 'showOptionsPage',
  SHOW_PANEL_MENU_MOST_RECENT: 'showPanelMenuMostRecent',
  SHOW_TABLES_LIST: 'showTablesList',
  SHOW_TALKBACK_KEYBOARD_SHORTCUTS: 'showTalkBackKeyboardShortcuts',
  SHOW_TTS_SETTINGS: 'showTtsSettings',
  SPEAK_TABLE_LOCATION: 'speakTableLocation',
  SPEAK_TIME_AND_DATE: 'speakTimeAndDate',
  START_HISTORY_RECORDING: 'startHistoryRecording',
  STOP_HISTORY_RECORDING: 'stopHistoryRecording',
  STOP_SPEECH: 'stopSpeech',
  TOGGLE_BRAILLE_CAPTIONS: 'toggleBrailleCaptions',
  TOGGLE_BRAILLE_TABLE: 'toggleBrailleTable',
  TOGGLE_DICTATION: 'toggleDictation',
  TOGGLE_EARCONS: 'toggleEarcons',
  TOGGLE_KEYBOARD_HELP: 'toggleKeyboardHelp',
  TOGGLE_SCREEN: 'toggleScreen',
  TOGGLE_SEARCH_WIDGET: 'toggleSearchWidget',
  TOGGLE_SELECTION: 'toggleSelection',
  TOGGLE_SEMANTICS: 'toggleSemantics',
  TOGGLE_SPEECH_ON_OR_OFF: 'toggleSpeechOnOrOff',
  TOGGLE_STICKY_MODE: 'toggleStickyMode',
  TOP: 'top',
  VIEW_GRAPHIC_AS_BRAILLE: 'viewGraphicAsBraille',
};

/**
 * List of categories for the commands.
 * Note that the values here must correspond to the message resource tag for the
 * category.
 * @enum {string}
 */
export const CommandCategory = {
  ACTIONS: 'actions',
  BRAILLE: 'braille',
  CONTROLLING_SPEECH: 'controlling_speech',
  DEVELOPER: 'developer',
  HELP_COMMANDS: 'help_commands',
  INFORMATION: 'information',
  JUMP_COMMANDS: 'jump_commands',
  MODIFIER_KEYS: 'modifier_keys',
  NAVIGATION: 'navigation',
  OVERVIEW: 'overview',
  TABLES: 'tables',
};

/**
 * Collection of command properties.
 * @type {Object<!Command, {
 *                  announce: boolean,
 *                  category: (undefined|!CommandCategory),
 *                  msgId: (undefined|string),
 *                  denySignedOut: (undefined|boolean)}>}
 *  announce: Whether to call finishNavCommand and announce the current
 *            position after the command is done.
 *  category: The command's category.
 *  msgId: The message resource describing the command.
 *  denySignedOut: Explicitly denies this command when on chrome://oobe/* or
 *             other signed-out contexts. Defaults to false.
 */
CommandStore.COMMAND_DATA = {
  [Command.TOGGLE_STICKY_MODE]: {
    announce: false,
    msgId: 'toggle_sticky_mode',
    category: CommandCategory.MODIFIER_KEYS,
  },
  [Command.PASS_THROUGH_MODE]: {
    announce: false,
    msgId: 'pass_through_key_description',
    category: CommandCategory.MODIFIER_KEYS,
  },

  [Command.STOP_SPEECH]: {
    announce: false,
    msgId: 'stop_speech_key',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.OPEN_CHROMEVOX_MENUS]: {announce: false, msgId: 'menus_title'},
  [Command.RESET_TEXT_TO_SPEECH_SETTINGS]: {
    announce: false,
    msgId: 'reset_tts_settings',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.DECREASE_TTS_RATE]: {
    announce: false,
    msgId: 'decrease_tts_rate',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.INCREASE_TTS_RATE]: {
    announce: false,
    msgId: 'increase_tts_rate',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.DECREASE_TTS_PITCH]: {
    announce: false,
    msgId: 'decrease_tts_pitch',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.INCREASE_TTS_PITCH]: {
    announce: false,
    msgId: 'increase_tts_pitch',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.DECREASE_TTS_VOLUME]: {
    announce: false,
    msgId: 'decrease_tts_volume',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.INCREASE_TTS_VOLUME]: {
    announce: false,
    msgId: 'increase_tts_volume',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.CYCLE_PUNCTUATION_ECHO]: {
    announce: false,
    msgId: 'cycle_punctuation_echo',
    category: CommandCategory.CONTROLLING_SPEECH,
  },
  [Command.CYCLE_TYPING_ECHO]: {
    announce: false,
    msgId: 'cycle_typing_echo',
    category: CommandCategory.CONTROLLING_SPEECH,
  },

  [Command.TOGGLE_DICTATION]: {
    announce: false,
    msgId: 'toggle_dictation',
    category: CommandCategory.ACTIONS,
  },

  [Command.TOGGLE_EARCONS]: {
    announce: true,
    msgId: 'toggle_earcons',
    category: CommandCategory.CONTROLLING_SPEECH,
  },

  [Command.TOGGLE_SPEECH_ON_OR_OFF]: {
    msgId: 'speech_on_off_description',
    category: CommandCategory.CONTROLLING_SPEECH,
  },

  [Command.HANDLE_TAB]: {
    msgId: 'handle_tab_next',
    category: CommandCategory.NAVIGATION,
  },
  [Command.HANDLE_TAB_PREV]: {
    msgId: 'handle_tab_prev',
    category: CommandCategory.NAVIGATION,
  },
  [Command.FORWARD]: {
    announce: true,
    msgId: 'forward',
    category: CommandCategory.NAVIGATION,
  },
  [Command.BACKWARD]: {
    announce: true,
    msgId: 'backward',
    category: CommandCategory.NAVIGATION,
  },
  [Command.RIGHT]: {
    announce: true,
    msgId: 'right',
    category: CommandCategory.NAVIGATION,
  },
  [Command.LEFT]: {
    announce: true,
    msgId: 'left',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_GRANULARITY]: {
    announce: true,
    msgId: 'previous_granularity',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_GRANULARITY]: {
    announce: true,
    msgId: 'next_granularity',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_AT_GRANULARITY]: {
    announce: true,
    msgId: 'previous_at_granularity',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_AT_GRANULARITY]: {
    announce: true,
    msgId: 'next_at_granularity',
    category: CommandCategory.NAVIGATION,
  },

  [Command.PREVIOUS_CHARACTER]: {
    announce: true,
    msgId: 'previous_character',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_CHARACTER]: {
    announce: true,
    msgId: 'next_character',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_WORD]: {
    announce: true,
    msgId: 'previous_word',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_WORD]: {
    announce: true,
    msgId: 'next_word',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_LINE]: {
    announce: true,
    msgId: 'previous_line',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_LINE]: {
    announce: true,
    msgId: 'next_line',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_SENTENCE]: {
    announce: true,
    msgId: 'previous_sentence',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_SENTENCE]: {
    announce: true,
    msgId: 'next_sentence',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_OBJECT]: {
    announce: true,
    msgId: 'previous_object',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_OBJECT]: {
    announce: true,
    msgId: 'next_object',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_GROUP]: {
    announce: true,
    msgId: 'previous_group',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_GROUP]: {
    announce: true,
    msgId: 'next_group',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_SIMILAR_ITEM]: {
    announce: true,
    msgId: 'previous_similar_item',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_SIMILAR_ITEM]: {
    announce: true,
    msgId: 'next_similar_item',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_INVALID_ITEM]: {
    announce: true,
    msgId: 'previous_invalid_item',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_INVALID_ITEM]: {
    announce: true,
    msgId: 'next_invalid_item',
    category: CommandCategory.NAVIGATION,
  },

  [Command.JUMP_TO_TOP]: {
    announce: true,
    msgId: 'jump_to_top',
    category: CommandCategory.NAVIGATION,
  },
  [Command.JUMP_TO_BOTTOM]: {
    announce: true,
    msgId: 'jump_to_bottom',
    category: CommandCategory.NAVIGATION,
  },
  // Intentionally uncategorized.
  [Command.MOVE_TO_START_OF_LINE]: {announce: true},
  [Command.MOVE_TO_END_OF_LINE]: {announce: true},

  [Command.JUMP_TO_DETAILS]: {
    announce: false,
    msgId: 'jump_to_details',
    category: CommandCategory.NAVIGATION,
  },

  [Command.READ_FROM_HERE]: {
    announce: false,
    msgId: 'read_from_here',
    category: CommandCategory.NAVIGATION,
  },

  [Command.FORCE_CLICK_ON_CURRENT_ITEM]: {
    announce: true,
    msgId: 'force_click_on_current_item',
    category: CommandCategory.ACTIONS,
  },
  [Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM]: {
    announce: true,
    msgId: 'force_long_click_on_current_item',
  },
  [Command.FORCE_DOUBLE_CLICK_ON_CURRENT_ITEM]: {announce: true},

  [Command.READ_LINK_URL]: {
    announce: false,
    msgId: 'read_link_url',
    category: CommandCategory.INFORMATION,
  },
  [Command.READ_CURRENT_TITLE]: {
    announce: false,
    msgId: 'read_current_title',
    category: CommandCategory.INFORMATION,
  },
  [Command.READ_CURRENT_URL]: {
    announce: false,
    msgId: 'read_current_url',
    category: CommandCategory.INFORMATION,
  },

  [Command.FULLY_DESCRIBE]: {
    announce: false,
    msgId: 'fully_describe',
    category: CommandCategory.INFORMATION,
  },
  [Command.SPEAK_TIME_AND_DATE]: {
    announce: false,
    msgId: 'speak_time_and_date',
    category: CommandCategory.INFORMATION,
  },
  [Command.TOGGLE_SELECTION]: {
    announce: true,
    msgId: 'toggle_selection',
    category: CommandCategory.ACTIONS,
  },

  [Command.TOGGLE_SEARCH_WIDGET]: {
    announce: false,
    msgId: 'toggle_search_widget',
    category: CommandCategory.INFORMATION,
  },

  [Command.TOGGLE_SCREEN]: {
    announce: false,
    msgId: 'toggle_screen',
    category: CommandCategory.MODIFIER_KEYS,
  },

  [Command.TOGGLE_BRAILLE_TABLE]:
      {msgId: 'toggle_braille_table', category: CommandCategory.HELP_COMMANDS},

  [Command.TOGGLE_KEYBOARD_HELP]: {
    announce: false,
    msgId: 'show_panel_menu',
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.SHOW_PANEL_MENU_MOST_RECENT]: {
    announce: false,
    msgId: 'show_panel_menu',
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.HELP]: {
    announce: false,
    msgId: 'help',
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.CONTEXT_MENU]: {
    announce: false,
    msgId: 'show_context_menu',
    category: CommandCategory.INFORMATION,
  },

  [Command.SHOW_OPTIONS_PAGE]: {
    announce: false,
    msgId: 'show_options_page',
    denySignedOut: true,
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.SHOW_LOG_PAGE]: {
    announce: false,
    msgId: 'show_log_page',
    denySignedOut: true,
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.SHOW_LEARN_MODE_PAGE]: {
    announce: false,
    msgId: 'show_kb_explorer_page',
    denySignedOut: true,
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.SHOW_TTS_SETTINGS]: {
    announce: false,
    msgId: 'show_tts_settings',
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
  },
  [Command.TOGGLE_BRAILLE_CAPTIONS]: {
    announce: false,
    msgId: 'braille_captions',
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.REPORT_ISSUE]: {
    announce: false,
    denySignedOut: true,
    msgId: 'panel_menu_item_report_issue',
    category: CommandCategory.HELP_COMMANDS,
  },

  [Command.SHOW_FORMS_LIST]: {
    announce: false,
    msgId: 'show_forms_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_HEADINGS_LIST]: {
    announce: false,
    msgId: 'show_headings_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_LANDMARKS_LIST]: {
    announce: false,
    msgId: 'show_landmarks_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_LINKS_LIST]: {
    announce: false,
    msgId: 'show_links_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_TABLES_LIST]: {
    announce: false,
    msgId: 'show_tables_list',
    category: CommandCategory.OVERVIEW,
  },

  [Command.NEXT_ARTICLE]: {},

  [Command.NEXT_BUTTON]: {
    msgId: 'next_button',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_CHECKBOX]: {
    msgId: 'next_checkbox',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_COMBO_BOX]: {
    msgId: 'next_combo_box',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_CONTROL]: {},
  [Command.NEXT_EDIT_TEXT]: {
    msgId: 'next_edit_text',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_FORM_FIELD]: {
    msgId: 'next_form_field',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_GRAPHIC]: {
    msgId: 'next_graphic',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING]: {
    msgId: 'next_heading',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_1]: {
    msgId: 'next_heading1',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_2]: {
    msgId: 'next_heading2',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_3]: {
    msgId: 'next_heading3',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_4]: {
    msgId: 'next_heading4',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_5]: {
    msgId: 'next_heading5',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_6]: {
    msgId: 'next_heading6',
    category: CommandCategory.JUMP_COMMANDS,
  },

  [Command.NEXT_LANDMARK]: {
    msgId: 'next_landmark',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_LINK]: {
    msgId: 'next_link',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_LIST]: {
    msgId: 'next_list',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_LIST_ITEM]: {
    msgId: 'next_list_item',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_MATH]: {
    msgId: 'next_math',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_MEDIA]: {
    msgId: 'next_media',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_RADIO]: {
    msgId: 'next_radio',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_SECTION]: {},
  [Command.NEXT_SLIDER]: {},
  [Command.NEXT_TABLE]: {
    msgId: 'next_table',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_VISITED_LINK]: {
    msgId: 'next_visited_link',
    category: CommandCategory.JUMP_COMMANDS,
  },


  [Command.PREVIOUS_ARTICLE]: {},

  [Command.PREVIOUS_BUTTON]: {
    msgId: 'previous_button',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_CHECKBOX]: {
    msgId: 'previous_checkbox',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_COMBO_BOX]: {
    msgId: 'previous_combo_box',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_CONTROL]: {},
  [Command.PREVIOUS_EDIT_TEXT]: {
    msgId: 'previous_edit_text',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_FORM_FIELD]: {
    msgId: 'previous_form_field',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_GRAPHIC]: {
    msgId: 'previous_graphic',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING]: {
    msgId: 'previous_heading',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_1]: {
    msgId: 'previous_heading1',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_2]: {
    msgId: 'previous_heading2',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_3]: {
    msgId: 'previous_heading3',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_4]: {
    msgId: 'previous_heading4',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_5]: {
    msgId: 'previous_heading5',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_6]: {
    msgId: 'previous_heading6',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LANDMARK]: {
    msgId: 'previous_landmark',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LINK]: {
    msgId: 'previous_link',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LIST]: {
    msgId: 'previous_list',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LIST_ITEM]: {
    msgId: 'previous_list_item',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_MATH]: {
    msgId: 'previous_math',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_MEDIA]: {
    msgId: 'previous_media',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_RADIO]: {
    msgId: 'previous_radio',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_SECTION]: {},
  [Command.PREVIOUS_SLIDER]: {},
  [Command.PREVIOUS_TABLE]: {
    msgId: 'previous_table',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_VISITED_LINK]: {
    msgId: 'previous_visited_link',
    category: CommandCategory.JUMP_COMMANDS,
  },


  // Table Actions.
  [Command.ANNOUNCE_HEADERS]: {
    announce: false,
    msgId: 'announce_headers',
    category: CommandCategory.TABLES,
  },
  [Command.SPEAK_TABLE_LOCATION]: {
    announce: false,
    msgId: 'speak_table_location',
    category: CommandCategory.TABLES,
  },
  [Command.GO_TO_FIRST_CELL]: {
    announce: true,
    msgId: 'skip_to_beginning',
    category: CommandCategory.TABLES,
  },
  [Command.GO_TO_LAST_CELL]:
      {announce: true, msgId: 'skip_to_end', category: CommandCategory.TABLES},
  [Command.GO_TO_ROW_FIRST_CELL]: {
    announce: true,
    msgId: 'skip_to_row_beginning',
    category: CommandCategory.TABLES,
  },
  [Command.GO_TO_ROW_LAST_CELL]: {
    announce: true,
    msgId: 'skip_to_row_end',
    category: CommandCategory.TABLES,
  },
  [Command.GO_TO_COL_FIRST_CELL]: {
    announce: true,
    msgId: 'skip_to_col_beginning',
    category: CommandCategory.TABLES,
  },
  [Command.GO_TO_COL_LAST_CELL]: {
    announce: true,
    msgId: 'skip_to_col_end',
    category: CommandCategory.TABLES,
  },
  [Command.PREVIOUS_ROW]: {
    announce: true,
    msgId: 'skip_to_prev_row',
    category: CommandCategory.TABLES,
  },
  [Command.PREVIOUS_COL]: {
    announce: true,
    msgId: 'skip_to_prev_col',
    category: CommandCategory.TABLES,
  },
  [Command.NEXT_ROW]: {
    announce: true,
    msgId: 'skip_to_next_row',
    category: CommandCategory.TABLES,
  },
  [Command.NEXT_COL]: {
    announce: true,
    msgId: 'skip_to_next_col',
    category: CommandCategory.TABLES,
  },

  // Generic Actions.
  [Command.ENTER_SHIFTER]: {
    announce: true,
    msgId: 'enter_content',
    category: CommandCategory.NAVIGATION,
  },
  [Command.EXIT_SHIFTER]: {
    announce: true,
    msgId: 'exit_content',
    category: CommandCategory.NAVIGATION,
  },
  [Command.EXIT_SHIFTER_CONTENT]: {announce: true},

  [Command.OPEN_LONG_DESC]: {
    announce: false,
    msgId: 'open_long_desc',
    category: CommandCategory.INFORMATION,
  },

  [Command.PAUSE_ALL_MEDIA]: {
    announce: false,
    msgId: 'pause_all_media',
    category: CommandCategory.INFORMATION,
  },

  [Command.ANNOUNCE_BATTERY_DESCRIPTION]: {
    announce: true,
    msgId: 'announce_battery_description',
    category: CommandCategory.INFORMATION,
  },
  [Command.ANNOUNCE_RICH_TEXT_DESCRIPTION]: {
    announce: true,
    msgId: 'announce_rich_text_description',
    category: CommandCategory.INFORMATION,
  },
  [Command.READ_PHONETIC_PRONUNCIATION]: {
    announce: true,
    msgId: 'read_phonetic_pronunciation',
    category: CommandCategory.INFORMATION,
  },

  // Scrolling actions.
  [Command.SCROLL_BACKWARD]: {msgId: 'action_scroll_backward_description'},
  [Command.SCROLL_FORWARD]: {msgId: 'action_scroll_forward_description'},

  // Math specific commands.
  [Command.TOGGLE_SEMANTICS]: {
    announce: false,
    msgId: 'toggle_semantics',
    category: CommandCategory.INFORMATION,
  },

  // Braille specific commands.
  [Command.ROUTING]: {
    announce: false,
    msgId: 'braille_routing',
    category: CommandCategory.BRAILLE,
  },
  [Command.PAN_LEFT]: {
    announce: true,
    msgId: 'braille_pan_left',
    category: CommandCategory.BRAILLE,
  },
  [Command.PAN_RIGHT]: {
    announce: true,
    msgId: 'braille_pan_right',
    category: CommandCategory.BRAILLE,
  },
  [Command.LINE_UP]: {
    announce: true,
    msgId: 'braille_line_up',
    category: CommandCategory.BRAILLE,
  },
  [Command.LINE_DOWN]: {
    announce: true,
    msgId: 'braille_line_down',
    category: CommandCategory.BRAILLE,
  },
  [Command.TOP]: {
    announce: true,
    msgId: 'braille_top',
    category: CommandCategory.BRAILLE,
  },
  [Command.BOTTOM]: {
    announce: true,
    msgId: 'braille_bottom',
    category: CommandCategory.BRAILLE,
  },
  [Command.VIEW_GRAPHIC_AS_BRAILLE]: {
    announce: true,
    msgId: 'view_graphic_as_braille',
    category: CommandCategory.BRAILLE,
  },

  // Developer commands.
  [Command.ENABLE_CONSOLE_TTS]: {
    announce: false,
    msgId: 'enable_tts_log',
    category: CommandCategory.DEVELOPER,
  },

  [Command.START_HISTORY_RECORDING]: {announce: false},
  [Command.STOP_HISTORY_RECORDING]: {announce: false},
  [Command.AUTORUNNER]: {announce: false},

  [Command.DEBUG]: {announce: false},

  [Command.NOP]: {announce: false},
};
