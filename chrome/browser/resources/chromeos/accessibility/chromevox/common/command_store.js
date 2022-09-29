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
 * 2. Add a command below in CommandStore.CMD_ALLOWLIST. Fill in each of the
 * relevant JSON keys.
 * Be sure to add a msg id and define it in chromevox/messages/messages.js which
 * describes the command. Please also add a category msg id so that the command
 * will show up in the options page.
 * 2. Add the command's logic to CommandHandler inside of our switch-based
 * dispatch method (onCommand).
 * 3. Add a key binding to KeySequence.
 *
 * Class description:
 * This class is entirely static and holds a JSON structure that stores
 * commands and their associated metadata.
 *
 * From this metadata, we compute relevant subsets of data such as all present
 * categories.
 */

export const CommandStore = {};

/**
 * Gets a message given a command.
 * @param {!Command} command The command to query.
 * @return {string|undefined} The message id, if any.
 */
CommandStore.messageForCommand = function(command) {
  return (CommandStore.CMD_ALLOWLIST[command] || {}).msgId;
};


/**
 * Gets a category given a command.
 * @param {!Command} command The command to query.
 * @return {string|undefined} The category, if any.
 */
CommandStore.categoryForCommand = function(command) {
  return (CommandStore.CMD_ALLOWLIST[command] || {}).category;
};

/**
 * Gets the first command associated with the message id
 * @param {string} msgId
 * @return {!Command|undefined} The command, if any.
 */
CommandStore.commandForMessage = function(msgId) {
  for (const commandName in CommandStore.CMD_ALLOWLIST) {
    const command = CommandStore.CMD_ALLOWLIST[commandName];
    if (command.msgId === msgId) {
      return commandName;
    }
  }
};

/**
 * Gets all commands for a category.
 * @param {string} category The category to query.
 * @return {Array<!Command>} The commands, if any.
 */
CommandStore.commandsForCategory = function(category) {
  const ret = [];
  for (const cmd in CommandStore.CMD_ALLOWLIST) {
    const struct = CommandStore.CMD_ALLOWLIST[cmd];
    if (category === struct.category) {
      ret.push(cmd);
    }
  }
  return ret;
};

/**
 * @param {!Command} command The command to query.
 * @return {boolean} Whether or not this command is denied in the OOBE.
 */
CommandStore.denySignedOut = function(command) {
  if (!CommandStore.CMD_ALLOWLIST[command]) {
    return false;
  }
  return Boolean(CommandStore.CMD_ALLOWLIST[command].denySignedOut);
};

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
 *                  forward: (undefined|boolean),
 *                  backward: (undefined|boolean),
 *                  announce: boolean,
 *                  category: (undefined|!CommandCategory),
 *                  findNext: (undefined|string),
 *                  doDefault: (undefined|boolean),
 *                  msgId: (undefined|string),
 *                  nodeList: (undefined|string),
 *                  skipInput: (undefined|boolean),
 *                  allowEvents: (undefined|boolean),
 *                  denyContinuation: (undefined|boolean),
 *                  denySignedOut: (undefined|boolean)}>}
 *  forward: Whether this command points forward.
 *  backward: Whether this command points backward. If neither forward or
 *            backward are specified, it stays facing in the current direction.
 *  announce: Whether to call finishNavCommand and announce the current
 *            position after the command is done.
 *  findNext: The id from the map above if this command is used for
 *            finding next/previous of something.
 *  category: The command's category.
 *  doDefault: Whether to do the default action. This means that keys will be
 *             passed through to the usual DOM capture/bubble phases.
 *  msgId: The message resource describing the command.
 *  nodeList: The id from the map above if this command is used for
 *            showing a list of nodes.
 *  skipInput: Explicitly skips this command when text input has focus.
 *             Defaults to false.
 *  denySignedOut: Explicitly denies this command when on chrome://oobe/* or
 *             other signed-out contexts. Defaults to false.
 *  allowEvents: Allows EventWatcher to continue processing events which can
 * trump TTS.
 *  denyContinuation: denies continuous read to proceed. Defaults to
 * false.
 */
CommandStore.CMD_ALLOWLIST = {
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
    denyContinuation: true,
    doDefault: true,
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
    allowEvents: true,
    msgId: 'handle_tab_next',
    denyContinuation: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.HANDLE_TAB_PREV]: {
    allowEvents: true,
    msgId: 'handle_tab_prev',
    denyContinuation: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.FORWARD]: {
    forward: true,
    announce: true,
    msgId: 'forward',
    category: CommandCategory.NAVIGATION,
  },
  [Command.BACKWARD]: {
    backward: true,
    announce: true,
    msgId: 'backward',
    category: CommandCategory.NAVIGATION,
  },
  [Command.RIGHT]: {
    forward: true,
    announce: true,
    msgId: 'right',
    category: CommandCategory.NAVIGATION,
  },
  [Command.LEFT]: {
    backward: true,
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
    backward: true,
    announce: true,
    msgId: 'previous_character',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_CHARACTER]: {
    forward: true,
    announce: true,
    msgId: 'next_character',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_WORD]: {
    backward: true,
    announce: true,
    msgId: 'previous_word',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_WORD]: {
    forward: true,
    announce: true,
    msgId: 'next_word',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_LINE]: {
    backward: true,
    announce: true,
    msgId: 'previous_line',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_LINE]: {
    forward: true,
    announce: true,
    msgId: 'next_line',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_SENTENCE]: {
    backward: true,
    announce: true,
    msgId: 'previous_sentence',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_SENTENCE]: {
    forward: true,
    announce: true,
    msgId: 'next_sentence',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_OBJECT]: {
    backward: true,
    announce: true,
    msgId: 'previous_object',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_OBJECT]: {
    forward: true,
    announce: true,
    msgId: 'next_object',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_GROUP]: {
    backward: true,
    announce: true,
    msgId: 'previous_group',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_GROUP]: {
    forward: true,
    announce: true,
    msgId: 'next_group',
    skipInput: true,
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_SIMILAR_ITEM]: {
    backward: true,
    announce: true,
    msgId: 'previous_similar_item',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_SIMILAR_ITEM]: {
    forward: true,
    announce: true,
    msgId: 'next_similar_item',
    category: CommandCategory.NAVIGATION,
  },
  [Command.PREVIOUS_INVALID_ITEM]: {
    backward: true,
    announce: true,
    msgId: 'previous_invalid_item',
    category: CommandCategory.NAVIGATION,
  },
  [Command.NEXT_INVALID_ITEM]: {
    forward: true,
    announce: true,
    msgId: 'next_invalid_item',
    category: CommandCategory.NAVIGATION,
  },

  [Command.JUMP_TO_TOP]: {
    forward: true,
    announce: true,
    msgId: 'jump_to_top',
    category: CommandCategory.NAVIGATION,
  },
  [Command.JUMP_TO_BOTTOM]: {
    backward: true,
    announce: true,
    msgId: 'jump_to_bottom',
    category: CommandCategory.NAVIGATION,
  },
  // Intentionally uncategorized.
  [Command.MOVE_TO_START_OF_LINE]: {forward: true, announce: true},
  [Command.MOVE_TO_END_OF_LINE]: {backward: true, announce: true},

  [Command.JUMP_TO_DETAILS]: {
    announce: false,
    msgId: 'jump_to_details',
    category: CommandCategory.NAVIGATION,
  },

  [Command.READ_FROM_HERE]: {
    forward: true,
    announce: false,
    msgId: 'read_from_here',
    category: CommandCategory.NAVIGATION,
  },

  [Command.FORCE_CLICK_ON_CURRENT_ITEM]: {
    announce: true,
    denyContinuation: true,
    allowEvents: true,
    msgId: 'force_click_on_current_item',
    category: CommandCategory.ACTIONS,
  },
  [Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM]: {
    announce: true,
    denyContinuation: true,
    allowEvents: true,
    msgId: 'force_long_click_on_current_item',
  },
  [Command.FORCE_DOUBLE_CLICK_ON_CURRENT_ITEM]:
      {announce: true, allowEvents: true, denyContinuation: true},

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
    denyContinuation: true,
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
    denyContinuation: true,
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
    denyContinuation: true,
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.CONTEXT_MENU]: {
    announce: false,
    msgId: 'show_context_menu',
    denyContinuation: true,
    category: CommandCategory.INFORMATION,
  },

  [Command.SHOW_OPTIONS_PAGE]: {
    announce: false,
    denyContinuation: true,
    msgId: 'show_options_page',
    denySignedOut: true,
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.SHOW_LOG_PAGE]: {
    announce: false,
    denyContinuation: true,
    msgId: 'show_log_page',
    denySignedOut: true,
    category: CommandCategory.HELP_COMMANDS,
  },
  [Command.SHOW_LEARN_MODE_PAGE]: {
    announce: false,
    denyContinuation: true,
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
    denyContinuation: true,
    nodeList: 'formField',
    msgId: 'show_forms_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_HEADINGS_LIST]: {
    announce: false,
    nodeList: 'heading',
    denyContinuation: true,
    msgId: 'show_headings_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_LANDMARKS_LIST]: {
    announce: false,
    nodeList: 'landmark',
    denyContinuation: true,
    msgId: 'show_landmarks_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_LINKS_LIST]: {
    announce: false,
    nodeList: 'link',
    denyContinuation: true,
    msgId: 'show_links_list',
    category: CommandCategory.OVERVIEW,
  },
  [Command.SHOW_TABLES_LIST]: {
    announce: false,
    nodeList: 'table',
    denyContinuation: true,
    msgId: 'show_tables_list',
    category: CommandCategory.OVERVIEW,
  },

  [Command.NEXT_ARTICLE]: {forward: true, findNext: 'article'},

  [Command.NEXT_BUTTON]: {
    forward: true,
    findNext: 'button',
    msgId: 'next_button',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_CHECKBOX]: {
    forward: true,
    findNext: 'checkbox',
    msgId: 'next_checkbox',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_COMBO_BOX]: {
    forward: true,
    findNext: 'combobox',
    msgId: 'next_combo_box',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_CONTROL]: {forward: true, findNext: 'control'},
  [Command.NEXT_EDIT_TEXT]: {
    forward: true,
    findNext: 'editText',
    msgId: 'next_edit_text',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_FORM_FIELD]: {
    forward: true,
    findNext: 'formField',
    msgId: 'next_form_field',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_GRAPHIC]: {
    forward: true,
    findNext: 'graphic',
    msgId: 'next_graphic',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING]: {
    forward: true,
    findNext: 'heading',
    msgId: 'next_heading',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_1]: {
    forward: true,
    findNext: 'heading1',
    msgId: 'next_heading1',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_2]: {
    forward: true,
    findNext: 'heading2',
    msgId: 'next_heading2',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_3]: {
    forward: true,
    findNext: 'heading3',
    msgId: 'next_heading3',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_4]: {
    forward: true,
    findNext: 'heading4',
    msgId: 'next_heading4',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_5]: {
    forward: true,
    findNext: 'heading5',
    msgId: 'next_heading5',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_HEADING_6]: {
    forward: true,
    findNext: 'heading6',
    msgId: 'next_heading6',
    category: CommandCategory.JUMP_COMMANDS,
  },

  [Command.NEXT_LANDMARK]: {
    forward: true,
    findNext: 'landmark',
    msgId: 'next_landmark',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_LINK]: {
    forward: true,
    findNext: 'link',
    msgId: 'next_link',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_LIST]: {
    forward: true,
    findNext: 'list',
    msgId: 'next_list',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_LIST_ITEM]: {
    forward: true,
    findNext: 'listItem',
    msgId: 'next_list_item',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_MATH]: {
    forward: true,
    findNext: 'math',
    msgId: 'next_math',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_MEDIA]: {
    forward: true,
    findNext: 'media',
    msgId: 'next_media',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_RADIO]: {
    forward: true,
    findNext: 'radio',
    msgId: 'next_radio',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_SECTION]: {forward: true, findNext: 'section'},
  [Command.NEXT_SLIDER]: {forward: true, findNext: 'slider'},
  [Command.NEXT_TABLE]: {
    forward: true,
    findNext: 'table',
    msgId: 'next_table',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.NEXT_VISITED_LINK]: {
    forward: true,
    findNext: 'visitedLink',
    msgId: 'next_visited_link',
    category: CommandCategory.JUMP_COMMANDS,
  },


  [Command.PREVIOUS_ARTICLE]: {backward: true, findNext: 'article'},

  [Command.PREVIOUS_BUTTON]: {
    backward: true,
    findNext: 'button',
    msgId: 'previous_button',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_CHECKBOX]: {
    backward: true,
    findNext: 'checkbox',
    msgId: 'previous_checkbox',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_COMBO_BOX]: {
    backward: true,
    findNext: 'combobox',
    msgId: 'previous_combo_box',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_CONTROL]: {backward: true, findNext: 'control'},
  [Command.PREVIOUS_EDIT_TEXT]: {
    backward: true,
    findNext: 'editText',
    msgId: 'previous_edit_text',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_FORM_FIELD]: {
    backward: true,
    findNext: 'formField',
    msgId: 'previous_form_field',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_GRAPHIC]: {
    backward: true,
    findNext: 'graphic',
    msgId: 'previous_graphic',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING]: {
    backward: true,
    findNext: 'heading',
    msgId: 'previous_heading',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_1]: {
    backward: true,
    findNext: 'heading1',
    msgId: 'previous_heading1',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_2]: {
    backward: true,
    findNext: 'heading2',
    msgId: 'previous_heading2',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_3]: {
    backward: true,
    findNext: 'heading3',
    msgId: 'previous_heading3',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_4]: {
    backward: true,
    findNext: 'heading4',
    msgId: 'previous_heading4',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_5]: {
    backward: true,
    findNext: 'heading5',
    msgId: 'previous_heading5',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_HEADING_6]: {
    backward: true,
    findNext: 'heading6',
    msgId: 'previous_heading6',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LANDMARK]: {
    backward: true,
    findNext: 'landmark',
    msgId: 'previous_landmark',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LINK]: {
    backward: true,
    findNext: 'link',
    msgId: 'previous_link',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LIST]: {
    backward: true,
    findNext: 'list',
    msgId: 'previous_list',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_LIST_ITEM]: {
    backward: true,
    findNext: 'listItem',
    msgId: 'previous_list_item',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_MATH]: {
    backward: true,
    findNext: 'math',
    msgId: 'previous_math',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_MEDIA]: {
    backward: true,
    findNext: 'media',
    msgId: 'previous_media',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_RADIO]: {
    backward: true,
    findNext: 'radio',
    msgId: 'previous_radio',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_SECTION]: {backward: true, findNext: 'section'},
  [Command.PREVIOUS_SLIDER]: {backward: true, findNext: 'slider'},
  [Command.PREVIOUS_TABLE]: {
    backward: true,
    findNext: 'table',
    msgId: 'previous_table',
    category: CommandCategory.JUMP_COMMANDS,
  },
  [Command.PREVIOUS_VISITED_LINK]: {
    backward: true,
    findNext: 'visitedLink',
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
    backward: true,
    announce: true,
    skipInput: true,
    msgId: 'skip_to_prev_row',
    category: CommandCategory.TABLES,
  },
  [Command.PREVIOUS_COL]: {
    backward: true,
    announce: true,
    skipInput: true,
    msgId: 'skip_to_prev_col',
    category: CommandCategory.TABLES,
  },
  [Command.NEXT_ROW]: {
    forward: true,
    announce: true,
    skipInput: true,
    msgId: 'skip_to_next_row',
    category: CommandCategory.TABLES,
  },
  [Command.NEXT_COL]: {
    forward: true,
    announce: true,
    skipInput: true,
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
    allowEvents: true,
    msgId: 'braille_routing',
    category: CommandCategory.BRAILLE,
  },
  [Command.PAN_LEFT]: {
    backward: true,
    announce: true,
    msgId: 'braille_pan_left',
    category: CommandCategory.BRAILLE,
  },
  [Command.PAN_RIGHT]: {
    forward: true,
    announce: true,
    msgId: 'braille_pan_right',
    category: CommandCategory.BRAILLE,
  },
  [Command.LINE_UP]: {
    backward: true,
    announce: true,
    msgId: 'braille_line_up',
    category: CommandCategory.BRAILLE,
  },
  [Command.LINE_DOWN]: {
    forward: true,
    announce: true,
    msgId: 'braille_line_down',
    category: CommandCategory.BRAILLE,
  },
  [Command.TOP]: {
    forward: true,
    announce: true,
    msgId: 'braille_top',
    category: CommandCategory.BRAILLE,
  },
  [Command.BOTTOM]: {
    backward: true,
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


/**
 * List of find next commands and their associated data.
 * @type {Object<{predicate: string,
 *                typeMsg: string,
 *                forwardError: string,
 *                backwardError: string}>}
 *  predicate: The name of the predicate. This must be defined in DomPredicates.
 *  forwardError: The message id of the error string when moving forward.
 *  backwardError: The message id of the error string when moving backward.
 */
CommandStore.NODE_INFO_MAP = {
  'checkbox': {
    predicate: 'checkboxPredicate',
    forwardError: 'no_next_checkbox',
    backwardError: 'no_previous_checkbox',
    typeMsg: 'role_checkbox',
  },
  'radio': {
    predicate: 'radioPredicate',
    forwardError: 'no_next_radio_button',
    backwardError: 'no_previous_radio_button',
    typeMsg: 'role_radio',
  },
  'slider': {
    predicate: 'sliderPredicate',
    forwardError: 'no_next_slider',
    backwardError: 'no_previous_slider',
    typeMsg: 'role_slider',
  },
  'graphic': {
    predicate: 'graphicPredicate',
    forwardError: 'no_next_graphic',
    backwardError: 'no_previous_graphic',
    typeMsg: 'UNUSED',
  },
  'article': {
    predicate: 'articlePredicate',
    forwardError: 'no_next_ARTICLE',
    backwardError: 'no_previous_ARTICLE',
    typeMsg: 'TAG_ARTICLE',
  },
  'button': {
    predicate: 'buttonPredicate',
    forwardError: 'no_next_button',
    backwardError: 'no_previous_button',
    typeMsg: 'role_button',
  },
  'combobox': {
    predicate: 'comboBoxPredicate',
    forwardError: 'no_next_combo_box',
    backwardError: 'no_previous_combo_box',
    typeMsg: 'role_combobox',
  },
  'editText': {
    predicate: 'editTextPredicate',
    forwardError: 'no_next_edit_text',
    backwardError: 'no_previous_edit_text',
    typeMsg: 'input_type_text',
  },
  'heading': {
    predicate: 'headingPredicate',
    forwardError: 'no_next_heading',
    backwardError: 'no_previous_heading',
    typeMsg: 'role_heading',
  },
  'heading1': {
    predicate: 'heading1Predicate',
    forwardError: 'no_next_heading_1',
    backwardError: 'no_previous_heading_1',
  },
  'heading2': {
    predicate: 'heading2Predicate',
    forwardError: 'no_next_heading_2',
    backwardError: 'no_previous_heading_2',
  },
  'heading3': {
    predicate: 'heading3Predicate',
    forwardError: 'no_next_heading_3',
    backwardError: 'no_previous_heading_3',
  },
  'heading4': {
    predicate: 'heading4Predicate',
    forwardError: 'no_next_heading_4',
    backwardError: 'no_previous_heading_4',
  },
  'heading5': {
    predicate: 'heading5Predicate',
    forwardError: 'no_next_heading_5',
    backwardError: 'no_previous_heading_5',
  },
  'heading6': {
    predicate: 'heading6Predicate',
    forwardError: 'no_next_heading_6',
    backwardError: 'no_previous_heading_6',
  },

  'link': {
    predicate: 'linkPredicate',
    forwardError: 'no_next_link',
    backwardError: 'no_previous_link',
    typeMsg: 'role_link',
  },
  'table': {
    predicate: 'tablePredicate',
    forwardError: 'no_next_table',
    backwardError: 'no_previous_table',
    typeMsg: 'table_strategy',
  },
  'visitedLink': {
    predicate: 'visitedLinkPredicate',
    forwardError: 'no_next_visited_link',
    backwardError: 'no_previous_visited_link',
    typeMsg: 'role_link',
  },
  'list': {
    predicate: 'listPredicate',
    forwardError: 'no_next_list',
    backwardError: 'no_previous_list',
    typeMsg: 'role_list',
  },
  'listItem': {
    predicate: 'listItemPredicate',
    forwardError: 'no_next_list_item',
    backwardError: 'no_previous_list_item',
    typeMsg: 'role_listitem',
  },
  'formField': {
    predicate: 'formFieldPredicate',
    forwardError: 'no_next_form_field',
    backwardError: 'no_previous_form_field',
    typeMsg: 'role_form',
  },
  'landmark': {
    predicate: 'landmarkPredicate',
    forwardError: 'no_next_landmark',
    backwardError: 'no_previous_landmark',
    typeMsg: 'role_landmark',
  },
  'math': {
    predicate: 'mathPredicate',
    forwardError: 'no_next_math',
    backwardError: 'no_previous_math',
    typeMsg: 'math_expr',
  },
  'media': {
    predicate: 'mediaPredicate',
    forwardError: 'no_next_media_widget',
    backwardError: 'no_previous_media_widget',
  },
  'section': {
    predicate: 'sectionPredicate',
    forwardError: 'no_next_section',
    backwardError: 'no_previous_section',
  },
  'control': {
    predicate: 'controlPredicate',
    forwardError: 'no_next_control',
    backwardError: 'no_previous_control',
  },
};
