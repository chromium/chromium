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
 * 1. Add the command to the |Command| enum in command.js.
 * 2. Add a command below in COMMAND_DATA. Fill in each of the relevant JSON
 * keys.
 * Be sure to add a msg id and define it in chromevox/messages/messages.js which
 * describes the command. Please also add a category msg id so that the command
 * will show up in the options page.
 * 2. Add the command's logic to CommandHandler inside of our switch-based
 * dispatch method (onCommand).
 * 3. Add a key binding to KeySequence.
 */

import {KeyCode} from '/common/key_code.js';

import {Command, CommandCategory} from './command.js';
import {KeyBinding, KeySequence, SerializedKeySequence} from './key_sequence.js';

export class CommandStore {
  /**
   * Gets a message given a command.
   * @param command The command to query.
   * @return The message id, if any.
   */
  static messageForCommand(command: Command): string | undefined {
    return COMMAND_DATA[command]?.msgId;
  }

  /**
   * Gets a category given a command.
   * @param command The command to query.
   * @return The category, if any.
   */
  static categoryForCommand(command: Command): CommandCategory | undefined {
    return COMMAND_DATA[command]?.category;
  }

  /**
   * Gets the first command associated with the message id
   * @return The command, if any.
   */
  static commandForMessage(msgId: string): Command | void {
    for (const commandName in COMMAND_DATA) {
      const command = COMMAND_DATA[commandName as Command];
      if (command.msgId === msgId) {
        return commandName as Command;
      }
    }
  }

  /**
   * Gets all commands for a category.
   * @param category The category to query.
   * @return The commands, if any.
   */
  static commandsForCategory(category: CommandCategory): Command[] {
    const ret: Command[] = [];
    for (const cmd in COMMAND_DATA) {
      const struct = COMMAND_DATA[cmd as Command];
      if (category === struct.category) {
        ret.push(cmd as Command);
      }
    }
    return ret;
  }

  /**
   * @param command The command to query.
   * @return Whether this command is denied in signed out contexts.
   */
  static denySignedOut(command: Command): boolean {
    return Boolean(COMMAND_DATA[command]?.denySignedOut);
  }

  static getKeyBindings(): KeyBinding[] {
    const primaryKeyBindings: KeyBinding[] =
        Object.entries(COMMAND_DATA)
            .filter(([_command, data]) => data.sequence)
            .map(([command, data]) => {
              // Always true, but closure compiler doesn't know that.
              if (data.sequence) {
                return {
                  command,
                  sequence: KeySequence.deserialize(data.sequence),
                };
              }
              return undefined;
            }) as KeyBinding[];

    const secondaryKeyBindings: KeyBinding[] =
        Object.entries(COMMAND_DATA)
            .filter(([_command, data]) => data.altSequence)
            .map(([command, data]) => {
              // Always true, but closure compiler doesn't know that.
              if (data.altSequence) {
                return {
                  command,
                  sequence: KeySequence.deserialize(data.altSequence),
                };
              }
              return undefined;
            }) as KeyBinding[];

    return primaryKeyBindings.concat(secondaryKeyBindings);
  }
}

/**
 *  category: The command's category.
 *  msgId: The message resource describing the command.
 *  denySignedOut: Explicitly denies this command when on chrome://oobe/* or
 *             other signed-out contexts. Defaults to false.
 */
interface DataEntry {
  category: CommandCategory;
  msgId?: string;
  denySignedOut?: boolean;
  sequence?: SerializedKeySequence;
  altSequence?: SerializedKeySequence;
}

/**
 * Collection of command properties.
 * NOTE: sorted in lookup order when matching against commands.
 */
export const COMMAND_DATA: Record<Command, DataEntry> = {
  [Command.ANNOUNCE_BATTERY_DESCRIPTION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'announce_battery_description',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.B]}},
  },
  [Command.ANNOUNCE_HEADERS]: {
    category: CommandCategory.TABLES,
    msgId: 'announce_headers',
  },
  [Command.ANNOUNCE_RICH_TEXT_DESCRIPTION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'announce_rich_text_description',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.F]}},
  },
  [Command.AUTORUNNER]: {category: CommandCategory.NO_CATEGORY},
  [Command.BACKWARD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'backward',
  },
  [Command.BOTTOM]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_bottom',
  },
  [Command.CONTEXT_MENU]: {
    category: CommandCategory.INFORMATION,
    msgId: 'show_context_menu',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.M]}},
  },
  [Command.COPY]: {
    category: CommandCategory.NO_CATEGORY,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], ctrlKey: [true]}},
  },
  [Command.CYCLE_PUNCTUATION_ECHO]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'cycle_punctuation_echo',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.P]}},
  },
  [Command.CYCLE_TYPING_ECHO]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'cycle_typing_echo',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.T]}},
  },
  [Command.DEBUG]: {category: CommandCategory.NO_CATEGORY},
  [Command.DECREASE_TTS_PITCH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_pitch',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_6], shiftKey: [true]},
    },
  },
  [Command.DECREASE_TTS_RATE]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_rate',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_4], shiftKey: [true]},
    },
  },
  [Command.DECREASE_TTS_VOLUME]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_volume',
  },
  [Command.DISABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_6]}},
  },
  [Command.DISABLE_LOGGING]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.D]}},
  },
  [Command.DUMP_TREE]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.D, KeyCode.T], ctrlKey: [true]},
    },
  },
  [Command.ENABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_4]}},
  },
  [Command.ENABLE_CONSOLE_TTS]: {
    category: CommandCategory.DEVELOPER,
    msgId: 'enable_tts_log',
  },
  [Command.ENABLE_LOGGING]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.E]}},
  },
  [Command.ENTER_SHIFTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'enter_content',
  },
  [Command.EXIT_SHIFTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'exit_content',
  },
  [Command.EXIT_SHIFTER_CONTENT]: {category: CommandCategory.NO_CATEGORY},
  [Command.FORCE_CLICK_ON_CURRENT_ITEM]: {
    category: CommandCategory.ACTIONS,
    msgId: 'force_click_on_current_item',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}},
  },
  [Command.FORCE_DOUBLE_CLICK_ON_CURRENT_ITEM]: {
    category: CommandCategory.NO_CATEGORY,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}, doubleTap: true},
  },
  [Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'force_long_click_on_current_item',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.SPACE], shiftKey: [true]},
    },
  },
  [Command.FORWARD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'forward',
  },
  [Command.FULLY_DESCRIBE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'fully_describe',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.K]}},
  },
  [Command.GO_TO_COL_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_col_beginning',
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
  [Command.GO_TO_COL_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_col_end',
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
  [Command.GO_TO_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_beginning',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], altKey: [true], shiftKey: [true]},
    },
  },
  [Command.GO_TO_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_end',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], altKey: [true], shiftKey: [true]},
    },
  },
  [Command.GO_TO_ROW_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_row_beginning',
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
  [Command.GO_TO_ROW_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_row_end',
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
  [Command.HANDLE_TAB]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'handle_tab_next',
  },
  [Command.HANDLE_TAB_PREV]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'handle_tab_prev',
  },
  [Command.HELP]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'help',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.T]}},
  },
  [Command.INCREASE_TTS_PITCH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_pitch',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_6]}},
  },
  [Command.INCREASE_TTS_RATE]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_rate',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_4]}},
  },
  [Command.INCREASE_TTS_VOLUME]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_volume',
  },
  [Command.JUMP_TO_BOTTOM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_bottom',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true]}},
  },
  [Command.JUMP_TO_DETAILS]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_details',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.J]}},
  },
  [Command.JUMP_TO_TOP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_top',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true]}},
  },
  [Command.LEFT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'left',
  },
  [Command.LINE_DOWN]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_line_down',
  },
  [Command.LINE_UP]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_line_up',
  },
  [Command.MOVE_TO_START_OF_LINE]: {category: CommandCategory.NO_CATEGORY},
  [Command.MOVE_TO_END_OF_LINE]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_ARTICLE]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_AT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_at_granularity',
  },
  [Command.NEXT_BUTTON]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_button',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.B]}},
  },
  [Command.NEXT_CHARACTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_character',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], shiftKey: [true]},
    },
  },
  [Command.NEXT_CHECKBOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_checkbox',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.X]}},
  },
  [Command.NEXT_COL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_next_col',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], altKey: [true]},
    },
  },
  [Command.NEXT_COMBO_BOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_combo_box',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.C]}},
  },
  [Command.NEXT_CONTROL]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_EDIT_TEXT]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_edit_text',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.E]}},
  },
  [Command.NEXT_FORM_FIELD]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_form_field',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.F]}},
  },
  [Command.NEXT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_granularity',
  },
  [Command.NEXT_GRAPHIC]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_graphic',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.G]}},
  },
  [Command.NEXT_GROUP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_group',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true]}},
  },
  [Command.NEXT_HEADING]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.H]}},
  },
  [Command.NEXT_HEADING_1]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading1',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE]}},
  },
  [Command.NEXT_HEADING_2]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading2',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO]}},
  },
  [Command.NEXT_HEADING_3]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading3',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.THREE]}},
  },
  [Command.NEXT_HEADING_4]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading4',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR]}},
  },
  [Command.NEXT_HEADING_5]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading5',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE]}},
  },
  [Command.NEXT_HEADING_6]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading6',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX]}},
  },
  [Command.NEXT_INVALID_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_invalid_item',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.N, KeyCode.I]}},
  },
  [Command.NEXT_LANDMARK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_landmark',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1]}},
  },
  [Command.NEXT_LINE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_line',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN]}},
  },
  [Command.NEXT_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_link',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.L]}},
  },
  [Command.NEXT_LIST]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_list',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.J, KeyCode.L]}},
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
  [Command.NEXT_OBJECT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_object',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT]}},
  },
  [Command.NEXT_PAGE]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_RADIO]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_radio',
  },
  [Command.NEXT_ROW]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_next_row',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true], altKey: [true]},
    },
  },
  [Command.NEXT_SECTION]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_SENTENCE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_sentence',
  },
  [Command.NEXT_SIMILAR_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_similar_item',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.I]}},
  },
  [Command.NEXT_SLIDER]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_TABLE]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_table',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.T]}},
  },
  [Command.NEXT_VISITED_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_visited_link',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.V]}},
  },
  [Command.NEXT_WORD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_word',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], shiftKey: [true]},
    },
  },
  [Command.NOP]: {category: CommandCategory.NO_CATEGORY},
  [Command.OPEN_CHROMEVOX_MENUS]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'menus_title',
  },
  [Command.OPEN_KEYBOARD_SHORTCUTS]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'open_keyboard_shortcuts_menu',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.S], ctrlKey: [true]}},
  },
  [Command.OPEN_LONG_DESC]: {
    category: CommandCategory.INFORMATION,
    msgId: 'open_long_desc',
  },
  [Command.PAN_LEFT]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_pan_left',
  },
  [Command.PAN_RIGHT]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_pan_right',
  },
  [Command.PASS_THROUGH_MODE]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'pass_through_key_description',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.ESCAPE], shiftKey: [true]},
    },
  },
  [Command.PAUSE_ALL_MEDIA]: {
    category: CommandCategory.INFORMATION,
    msgId: 'pause_all_media',
  },
  [Command.PREVIOUS_ARTICLE]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_AT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_at_granularity',
  },
  [Command.PREVIOUS_BUTTON]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_button',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.B], shiftKey: [true]}},
  },
  [Command.PREVIOUS_CHARACTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_character',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], shiftKey: [true]}},
  },
  [Command.PREVIOUS_CHECKBOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_checkbox',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.X], shiftKey: [true]}},
  },
  [Command.PREVIOUS_COL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_prev_col',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], altKey: [true]},
    },
  },
  [Command.PREVIOUS_COMBO_BOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_combo_box',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], shiftKey: [true]}},
  },
  [Command.PREVIOUS_CONTROL]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_EDIT_TEXT]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_edit_text',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.E], shiftKey: [true]}},
  },
  [Command.PREVIOUS_FORM_FIELD]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_form_field',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], shiftKey: [true]}},
  },
  [Command.PREVIOUS_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_granularity',
  },
  [Command.PREVIOUS_GRAPHIC]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_graphic',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.G], shiftKey: [true]}},
  },
  [Command.PREVIOUS_GROUP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_group',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.UP], ctrlKey: [true]}},
  },
  [Command.PREVIOUS_HEADING]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], shiftKey: [true]}},
  },
  [Command.PREVIOUS_HEADING_1]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading1',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE], shiftKey: [true]}},
  },
  [Command.PREVIOUS_HEADING_2]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading2',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO], shiftKey: [true]}},
  },
  [Command.PREVIOUS_HEADING_3]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading3',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.THREE], shiftKey: [true]},
    },
  },
  [Command.PREVIOUS_HEADING_4]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading4',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR], shiftKey: [true]}},
  },
  [Command.PREVIOUS_HEADING_5]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading5',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE], shiftKey: [true]}},
  },
  [Command.PREVIOUS_HEADING_6]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading6',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX], shiftKey: [true]}},
  },
  [Command.PREVIOUS_INVALID_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_invalid_item',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.P, KeyCode.I]}},
  },
  [Command.PREVIOUS_LANDMARK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_landmark',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_1], shiftKey: [true]},
    },
  },
  [Command.PREVIOUS_LINE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_line',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.UP]}},
  },
  [Command.PREVIOUS_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_link',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], shiftKey: [true]}},
  },
  [Command.PREVIOUS_LIST]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_list',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.J, KeyCode.L], shiftKey: [true]},
    },
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
  [Command.PREVIOUS_OBJECT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_object',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT]}},
  },
  [Command.PREVIOUS_PAGE]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_RADIO]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_radio',
  },
  [Command.PREVIOUS_ROW]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_prev_row',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.UP], ctrlKey: [true], altKey: [true]},
    },
  },
  [Command.PREVIOUS_SECTION]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_SENTENCE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_sentence',
  },
  [Command.PREVIOUS_SIMILAR_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_similar_item',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.I], shiftKey: [true]}},
  },
  [Command.PREVIOUS_SLIDER]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_TABLE]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_table',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], shiftKey: [true]}},
  },
  [Command.PREVIOUS_VISITED_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_visited_link',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.V], shiftKey: [true]}},
  },
  [Command.PREVIOUS_WORD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_word',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], shiftKey: [true]},
    },
  },
  [Command.READ_CURRENT_TITLE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_current_title',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.W]}},
  },
  [Command.READ_CURRENT_URL]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_current_url',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.U]}},
  },
  [Command.READ_FROM_HERE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'read_from_here',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.R]}},
  },
  [Command.READ_LINK_URL]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_link_url',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.L]}},
  },
  [Command.READ_PHONETIC_PRONUNCIATION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_phonetic_pronunciation',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.C]}},
  },
  [Command.REPORT_ISSUE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'panel_menu_item_report_issue',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.I]}},
  },
  [Command.RESET_TEXT_TO_SPEECH_SETTINGS]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'reset_tts_settings',
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_5], ctrlKey: [true], shiftKey: [true]},
    },
  },
  [Command.RIGHT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'right',
  },
  [Command.ROUTING]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_routing',
  },
  [Command.SCROLL_BACKWARD]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'action_scroll_backward_description',
  },
  [Command.SCROLL_FORWARD]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'action_scroll_forward_description',
  },
  [Command.SHOW_ACTIONS_MENU]: {
    category: CommandCategory.NO_CATEGORY,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.A], ctrlKey: [true]}},
  },
  [Command.SHOW_FORMS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_forms_list',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], ctrlKey: [true]}},
  },
  [Command.SHOW_HEADINGS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_headings_list',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], ctrlKey: [true]}},
  },
  [Command.SHOW_LANDMARKS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_landmarks_list',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1], ctrlKey: [true]}},
  },
  [Command.SHOW_LEARN_MODE_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_kb_explorer_page',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.K]}},
  },
  [Command.SHOW_LINKS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_links_list',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], ctrlKey: [true]}},
  },
  [Command.SHOW_LOG_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_log_page',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.W]}},
  },
  [Command.SHOW_OPTIONS_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_options_page',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.O]}},
  },
  [Command.SHOW_PANEL_MENU_MOST_RECENT]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_panel_menu',
  },
  [Command.SHOW_TABLES_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_tables_list',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], ctrlKey: [true]}},
  },
  [Command.SHOW_TALKBACK_KEYBOARD_SHORTCUTS]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.K]}},
  },
  [Command.SHOW_TTS_SETTINGS]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_tts_settings',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.S]}},
  },
  [Command.SPEAK_TABLE_LOCATION]: {
    category: CommandCategory.TABLES,
    msgId: 'speak_table_location',
  },
  [Command.SPEAK_TIME_AND_DATE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'speak_time_and_date',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.D]}},
  },
  [Command.START_HISTORY_RECORDING]: {category: CommandCategory.NO_CATEGORY},
  [Command.STOP_HISTORY_RECORDING]: {category: CommandCategory.NO_CATEGORY},
  [Command.STOP_SPEECH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'stop_speech_key',
    sequence: {
      cvoxModifier: false,
      keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]},
    },
    altSequence: {keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]}},
  },
  [Command.TOGGLE_BRAILLE_CAPTIONS]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'braille_captions',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.B]}},
  },
  [Command.TOGGLE_BRAILLE_TABLE]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'toggle_braille_table',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.G]}},
  },
  [Command.TOGGLE_DICTATION]: {
    category: CommandCategory.ACTIONS,
    msgId: 'toggle_dictation',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.D]}},
  },
  [Command.TOGGLE_EARCONS]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'toggle_earcons',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.E]}},
  },
  [Command.TOGGLE_KEYBOARD_HELP]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_panel_menu',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_PERIOD]}},
  },
  [Command.TOGGLE_SCREEN]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'toggle_screen',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.BRIGHTNESS_UP]}},
  },
  [Command.TOGGLE_SEARCH_WIDGET]: {
    category: CommandCategory.INFORMATION,
    msgId: 'toggle_search_widget',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_2]}},
  },
  [Command.TOGGLE_SELECTION]: {
    category: CommandCategory.ACTIONS,
    msgId: 'toggle_selection',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.S]}},
  },
  [Command.TOGGLE_SEMANTICS]: {
    category: CommandCategory.INFORMATION,
    msgId: 'toggle_semantics',
  },
  [Command.TOGGLE_SPEECH_ON_OR_OFF]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'speech_on_off_description',
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.VOLUME_MUTE]}},
  },
  [Command.TOGGLE_STICKY_MODE]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'toggle_sticky_mode',
    sequence: {
      skipStripping: false,
      doubleTap: true,
      keys: {keyCode: [KeyCode.SEARCH]},
    },
  },
  [Command.TOP]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_top',
  },
  [Command.VIEW_GRAPHIC_AS_BRAILLE]: {
    category: CommandCategory.BRAILLE,
    msgId: 'view_graphic_as_braille',
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.G], altKey: [true]}},
  },

  // Keep these commands last since they potentially conflict with above
  // commands when sticky mode is enabled.
  [Command.NATIVE_NEXT_CHARACTER]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {cvoxModifier: false, keys: {keyCode: [KeyCode.RIGHT]}},
  },
  [Command.NATIVE_NEXT_WORD]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {
      cvoxModifier: false,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true]},
    },
  },
  [Command.NATIVE_PREVIOUS_CHARACTER]: {
    category: CommandCategory.NO_CATEGORY,
    sequence: {cvoxModifier: false, keys: {keyCode: [KeyCode.LEFT]}},
  },
  [Command.NATIVE_PREVIOUS_WORD]: {
    category: CommandCategory.NO_CATEGORY,
    sequence:
        {cvoxModifier: false, keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true]}},
  },
};
