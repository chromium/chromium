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
    return CommandStore.COMMAND_DATA[command]?.msgId;
  }

  /**
   * Gets a category given a command.
   * @param {!Command} command The command to query.
   * @return {!CommandCategory|undefined} The category, if any.
   */
  static categoryForCommand(command) {
    return CommandStore.COMMAND_DATA[command]?.category;
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
   * @param {!CommandCategory} category The category to query.
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
  CONTROLLING_SPEECH: 'controlling_speech',
  HELP_COMMANDS: 'help_commands',
  INFORMATION: 'information',
  JUMP_COMMANDS: 'jump_commands',
  MODIFIER_KEYS: 'modifier_keys',
  NAVIGATION: 'navigation',
  OVERVIEW: 'overview',
  TABLES: 'tables',
  // The following categories are not displayed in the ChromeVox menus:
  BRAILLE: 'braille',
  DEVELOPER: 'developer',
  NO_CATEGORY: 'no_category',
};

/**
 * @typedef {{
 *     category: !CommandCategory,
 *     msgId: (undefined|string),
 *     denySignedOut: (undefined|boolean)
 * }}
 *  category: The command's category.
 *  msgId: The message resource describing the command.
 *  denySignedOut: Explicitly denies this command when on chrome://oobe/* or
 *             other signed-out contexts. Defaults to false.
 */
let DataEntry;

/**
 * Collection of command properties.
 * @type {Object<!Command, !DataEntry>}
 */
CommandStore.COMMAND_DATA = {
  [Command.TOGGLE_STICKY_MODE]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'toggle_sticky_mode',
  },
  [Command.PASS_THROUGH_MODE]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'pass_through_key_description',
  },
  [Command.STOP_SPEECH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'stop_speech_key',
  },
  [Command.OPEN_CHROMEVOX_MENUS]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'menus_title',
  },
  [Command.RESET_TEXT_TO_SPEECH_SETTINGS]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'reset_tts_settings',
  },
  [Command.DECREASE_TTS_RATE]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_rate',
  },
  [Command.INCREASE_TTS_RATE]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_rate',
  },
  [Command.DECREASE_TTS_PITCH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_pitch',
  },
  [Command.INCREASE_TTS_PITCH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_pitch',
  },
  [Command.DECREASE_TTS_VOLUME]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_volume',
  },
  [Command.INCREASE_TTS_VOLUME]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_volume',
  },
  [Command.CYCLE_PUNCTUATION_ECHO]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'cycle_punctuation_echo',
  },
  [Command.CYCLE_TYPING_ECHO]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'cycle_typing_echo',
  },
  [Command.TOGGLE_DICTATION]: {
    category: CommandCategory.ACTIONS,
    msgId: 'toggle_dictation',
  },
  [Command.TOGGLE_EARCONS]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'toggle_earcons',
  },
  [Command.TOGGLE_SPEECH_ON_OR_OFF]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'speech_on_off_description',
  },
  [Command.HANDLE_TAB]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'handle_tab_next',
  },
  [Command.HANDLE_TAB_PREV]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'handle_tab_prev',
  },
  [Command.FORWARD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'forward',
  },
  [Command.BACKWARD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'backward',
  },
  [Command.RIGHT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'right',
  },
  [Command.LEFT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'left',
  },
  [Command.PREVIOUS_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_granularity',
  },
  [Command.NEXT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_granularity',
  },
  [Command.PREVIOUS_AT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_at_granularity',
  },
  [Command.NEXT_AT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_at_granularity',
  },
  [Command.PREVIOUS_CHARACTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_character',
  },
  [Command.NEXT_CHARACTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_character',
  },
  [Command.PREVIOUS_WORD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_word',
  },
  [Command.NEXT_WORD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_word',
  },
  [Command.PREVIOUS_LINE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_line',
  },
  [Command.NEXT_LINE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_line',
  },
  [Command.PREVIOUS_SENTENCE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_sentence',
  },
  [Command.NEXT_SENTENCE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_sentence',
  },
  [Command.PREVIOUS_OBJECT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_object',
  },
  [Command.NEXT_OBJECT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_object',
  },
  [Command.PREVIOUS_GROUP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_group',
  },
  [Command.NEXT_GROUP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_group',
  },
  [Command.PREVIOUS_SIMILAR_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_similar_item',
  },
  [Command.NEXT_SIMILAR_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_similar_item',
  },
  [Command.PREVIOUS_INVALID_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_invalid_item',
  },
  [Command.NEXT_INVALID_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_invalid_item',
  },
  [Command.JUMP_TO_TOP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_top',
  },
  [Command.JUMP_TO_BOTTOM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_bottom',
  },

  // Intentionally uncategorized.
  [Command.MOVE_TO_START_OF_LINE]: {category: CommandCategory.NO_CATEGORY},
  [Command.MOVE_TO_END_OF_LINE]: {category: CommandCategory.NO_CATEGORY},

  [Command.JUMP_TO_DETAILS]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_details',
  },
  [Command.READ_FROM_HERE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'read_from_here',
  },
  [Command.FORCE_CLICK_ON_CURRENT_ITEM]: {
    category: CommandCategory.ACTIONS,
    msgId: 'force_click_on_current_item',
  },
  [Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'force_long_click_on_current_item',
  },
  [Command.FORCE_DOUBLE_CLICK_ON_CURRENT_ITEM]:
      {category: CommandCategory.NO_CATEGORY},
  [Command.READ_LINK_URL]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_link_url',
  },
  [Command.READ_CURRENT_TITLE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_current_title',
  },
  [Command.READ_CURRENT_URL]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_current_url',
  },
  [Command.FULLY_DESCRIBE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'fully_describe',
  },
  [Command.SPEAK_TIME_AND_DATE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'speak_time_and_date',
  },
  [Command.TOGGLE_SELECTION]: {
    category: CommandCategory.ACTIONS,
    msgId: 'toggle_selection',
  },
  [Command.TOGGLE_SEARCH_WIDGET]: {
    category: CommandCategory.INFORMATION,
    msgId: 'toggle_search_widget',
  },
  [Command.TOGGLE_SCREEN]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'toggle_screen',
  },
  [Command.TOGGLE_BRAILLE_TABLE]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'toggle_braille_table',
  },
  [Command.TOGGLE_KEYBOARD_HELP]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_panel_menu',
  },
  [Command.SHOW_PANEL_MENU_MOST_RECENT]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_panel_menu',
  },
  [Command.HELP]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'help',
  },
  [Command.CONTEXT_MENU]: {
    category: CommandCategory.INFORMATION,
    msgId: 'show_context_menu',
  },
  [Command.SHOW_OPTIONS_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_options_page',
  },
  [Command.SHOW_LOG_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_log_page',
  },
  [Command.SHOW_LEARN_MODE_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_kb_explorer_page',
  },
  [Command.SHOW_TTS_SETTINGS]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_tts_settings',
  },
  [Command.TOGGLE_BRAILLE_CAPTIONS]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'braille_captions',
  },
  [Command.REPORT_ISSUE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'panel_menu_item_report_issue',
  },
  [Command.SHOW_FORMS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_forms_list',
  },
  [Command.SHOW_HEADINGS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_headings_list',
  },
  [Command.SHOW_LANDMARKS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_landmarks_list',
  },
  [Command.SHOW_LINKS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_links_list',
  },
  [Command.SHOW_TABLES_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_tables_list',
  },
  [Command.NEXT_ARTICLE]: {category: CommandCategory.NO_CATEGORY},

  [Command.NEXT_BUTTON]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_button',
  },
  [Command.NEXT_CHECKBOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_checkbox',
  },
  [Command.NEXT_COMBO_BOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_combo_box',
  },
  [Command.NEXT_CONTROL]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_EDIT_TEXT]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_edit_text',
  },
  [Command.NEXT_FORM_FIELD]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_form_field',
  },
  [Command.NEXT_GRAPHIC]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_graphic',
  },
  [Command.NEXT_HEADING]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading',
  },
  [Command.NEXT_HEADING_1]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading1',
  },
  [Command.NEXT_HEADING_2]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading2',
  },
  [Command.NEXT_HEADING_3]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading3',
  },
  [Command.NEXT_HEADING_4]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading4',
  },
  [Command.NEXT_HEADING_5]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading5',
  },
  [Command.NEXT_HEADING_6]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading6',
  },
  [Command.NEXT_LANDMARK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_landmark',
  },
  [Command.NEXT_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_link',
  },
  [Command.NEXT_LIST]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_list',
  },
  [Command.NEXT_LIST_ITEM]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_list_item',
  },
  [Command.NEXT_MATH]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_math',
  },
  [Command.NEXT_MEDIA]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_media',
  },
  [Command.NEXT_RADIO]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_radio',
  },
  [Command.NEXT_SECTION]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_SLIDER]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_TABLE]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_table',
  },
  [Command.NEXT_VISITED_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_visited_link',
  },
  [Command.PREVIOUS_ARTICLE]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_BUTTON]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_button',
  },
  [Command.PREVIOUS_CHECKBOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_checkbox',
  },
  [Command.PREVIOUS_COMBO_BOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_combo_box',
  },
  [Command.PREVIOUS_CONTROL]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_EDIT_TEXT]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_edit_text',
  },
  [Command.PREVIOUS_FORM_FIELD]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_form_field',
  },
  [Command.PREVIOUS_GRAPHIC]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_graphic',
  },
  [Command.PREVIOUS_HEADING]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading',
  },
  [Command.PREVIOUS_HEADING_1]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading1',
  },
  [Command.PREVIOUS_HEADING_2]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading2',
  },
  [Command.PREVIOUS_HEADING_3]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading3',
  },
  [Command.PREVIOUS_HEADING_4]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading4',
  },
  [Command.PREVIOUS_HEADING_5]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading5',
  },
  [Command.PREVIOUS_HEADING_6]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading6',
  },
  [Command.PREVIOUS_LANDMARK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_landmark',
  },
  [Command.PREVIOUS_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_link',
  },
  [Command.PREVIOUS_LIST]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_list',
  },
  [Command.PREVIOUS_LIST_ITEM]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_list_item',
  },
  [Command.PREVIOUS_MATH]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_math',
  },
  [Command.PREVIOUS_MEDIA]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_media',
  },
  [Command.PREVIOUS_RADIO]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_radio',
  },
  [Command.PREVIOUS_SECTION]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_SLIDER]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_TABLE]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_table',
  },
  [Command.PREVIOUS_VISITED_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_visited_link',
  },

  // Table Actions.
  [Command.ANNOUNCE_HEADERS]: {
    category: CommandCategory.TABLES,
    msgId: 'announce_headers',
  },
  [Command.SPEAK_TABLE_LOCATION]: {
    category: CommandCategory.TABLES,
    msgId: 'speak_table_location',
  },
  [Command.GO_TO_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_beginning',
  },
  [Command.GO_TO_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_end',
  },
  [Command.GO_TO_ROW_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_row_beginning',
  },
  [Command.GO_TO_ROW_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_row_end',
  },
  [Command.GO_TO_COL_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_col_beginning',
  },
  [Command.GO_TO_COL_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_col_end',
  },
  [Command.PREVIOUS_ROW]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_prev_row',
  },
  [Command.PREVIOUS_COL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_prev_col',
  },
  [Command.NEXT_ROW]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_next_row',
  },
  [Command.NEXT_COL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_next_col',
  },

  // Generic Actions.
  [Command.ENTER_SHIFTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'enter_content',
  },
  [Command.EXIT_SHIFTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'exit_content',
  },
  [Command.EXIT_SHIFTER_CONTENT]: {category: CommandCategory.NO_CATEGORY},
  [Command.OPEN_LONG_DESC]: {
    category: CommandCategory.INFORMATION,
    msgId: 'open_long_desc',
  },
  [Command.PAUSE_ALL_MEDIA]: {
    category: CommandCategory.INFORMATION,
    msgId: 'pause_all_media',
  },
  [Command.ANNOUNCE_BATTERY_DESCRIPTION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'announce_battery_description',
  },
  [Command.ANNOUNCE_RICH_TEXT_DESCRIPTION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'announce_rich_text_description',
  },
  [Command.READ_PHONETIC_PRONUNCIATION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_phonetic_pronunciation',
  },

  // Scrolling actions.
  [Command.SCROLL_BACKWARD]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'action_scroll_backward_description',
  },
  [Command.SCROLL_FORWARD]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'action_scroll_forward_description',
  },

  // Math specific commands.
  [Command.TOGGLE_SEMANTICS]: {
    category: CommandCategory.INFORMATION,
    msgId: 'toggle_semantics',
  },

  // Braille specific commands.
  [Command.ROUTING]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_routing',
  },
  [Command.PAN_LEFT]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_pan_left',
  },
  [Command.PAN_RIGHT]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_pan_right',
  },
  [Command.LINE_UP]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_line_up',
  },
  [Command.LINE_DOWN]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_line_down',
  },
  [Command.TOP]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_top',
  },
  [Command.BOTTOM]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_bottom',
  },
  [Command.VIEW_GRAPHIC_AS_BRAILLE]: {
    category: CommandCategory.BRAILLE,
    msgId: 'view_graphic_as_braille',
  },

  // Developer commands.
  [Command.ENABLE_CONSOLE_TTS]: {
    category: CommandCategory.DEVELOPER,
    msgId: 'enable_tts_log',
  },
  [Command.START_HISTORY_RECORDING]: {category: CommandCategory.NO_CATEGORY},
  [Command.STOP_HISTORY_RECORDING]: {category: CommandCategory.NO_CATEGORY},
  [Command.AUTORUNNER]: {category: CommandCategory.NO_CATEGORY},
  [Command.DEBUG]: {category: CommandCategory.NO_CATEGORY},
  [Command.NOP]: {category: CommandCategory.NO_CATEGORY},
};
