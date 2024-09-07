// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox commands.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {BridgeHelper} from '/common/bridge_helper.js';
import {BrowserUtil} from '/common/browser_util.js';
import {constants} from '/common/constants.js';
import {CursorUnit} from '/common/cursors/cursor.js';
import {CursorRange} from '/common/cursors/range.js';
import {EventGenerator} from '/common/event_generator.js';
import {KeyCode} from '/common/key_code.js';
import {LocalStorage} from '/common/local_storage.js';
import {RectUtil} from '/common/rect_util.js';

import {NavBraille} from '../../common/braille/nav_braille.js';
import {BridgeConstants} from '../../common/bridge_constants.js';
import {Command} from '../../common/command.js';
import {EarconId} from '../../common/earcon_id.js';
import {EventSourceType} from '../../common/event_source_type.js';
import {GestureGranularity} from '../../common/gesture_command_data.js';
import {ChromeVoxKbHandler} from '../../common/keyboard_handler.js';
import {Msgs} from '../../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../../common/panel_command.js';
import {PermissionChecker} from '../../common/permission_checker.js';
import {QueueMode, TtsSettings, TtsSpeechProperties} from '../../common/tts_types.js';
import {AutoScrollHandler} from '../auto_scroll_handler.js';
import {BrailleCaptionsBackground} from '../braille/braille_captions_background.js';
import {BrailleTranslatorManager} from '../braille/braille_translator_manager.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange} from '../chromevox_range.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {Color} from '../color.js';
import {TypingEcho} from '../editing/typing_echo.js';
import {DesktopAutomationInterface} from '../event/desktop_automation_interface.js';
import {EventSource} from '../event_source.js';
import {LogManager} from '../logging/log_manager.js';
import {Output} from '../output/output.js';
import {OutputCustomEvent} from '../output/output_types.js';
import {PhoneticData} from '../phonetic_data.js';
import {ChromeVoxPrefs} from '../prefs.js';
import {TtsBackground} from '../tts_background.js';

import {BackgroundKeyboardHandler} from './background_keyboard_handler.js';
import {ClipboardHandler} from './clipboard_handler.js';
import {CommandHandlerInterface} from './command_handler_interface.js';
import {GestureInterface} from './gesture_interface.js';
import {SmartStickyMode} from './smart_sticky_mode.js';

import AutomationNode = chrome.automation.AutomationNode;
type CreateType = chrome.windows.CreateType;
import Dir = constants.Dir;
const Restriction = chrome.automation.Restriction;
const RoleType = chrome.automation.RoleType;
const SetNativeChromeVoxResponse =
    chrome.accessibilityPrivate.SetNativeChromeVoxResponse;
const StateType = chrome.automation.StateType;

interface NewRangeData {
  node?: AutomationNode;
  range: CursorRange;
}

/**
 * Maps a Command to the method that will perform that action.
 *
 * To streamline this class, the goal is to move the logic for each command out
 * of this file.
 *
 * When adding new commands, please put the logic in a more relevant spot.
 */
export class CommandHandler implements CommandHandlerInterface {
  private commandList_ = Object.values(Command);

  onCommand(command: Command): boolean {
    // Check for a command denied in incognito contexts and kiosk.
    if (!PermissionChecker.isAllowed(command)) {
      return true;
    }

    chrome.metricsPrivate.recordEnumerationValue(
        'Accessibility.ChromeVox.PerformCommand',
        this.commandList_.indexOf(command), this.commandList_.length);

    ChromeVoxRange.maybeResetFromFocus();

    // These commands don't require a current range.
    switch (command) {
      case Command.SPEAK_TIME_AND_DATE:
        this.speakTimeAndDate_();
        return false;
      case Command.SHOW_OPTIONS_PAGE:
        this.showOptionsPage_();
        break;
      case Command.TOGGLE_STICKY_MODE:
        // TODO(b/314203187): Not null asserted, check that this is correct.
        SmartStickyMode.instance!.toggle();
        return false;
      case Command.PASS_THROUGH_MODE:
        if (ChromeVoxPrefs.isStickyModeOn()) {
          new Output()
              .withString(
                  Msgs.getMsg('pass_through_unavailable_with_sticky_mode'))
              .go();
        } else {
          BackgroundKeyboardHandler.enablePassThroughMode();
        }
        return true;
      case Command.SHOW_LEARN_MODE_PAGE:
        this.showLearnModePage_();
        break;
      case Command.SHOW_LOG_PAGE:
        LogManager.showLogPage();
        break;
      case Command.ENABLE_LOGGING:
        LogManager.setLoggingEnabled(true);
        break;
      case Command.DISABLE_LOGGING:
        LogManager.setLoggingEnabled(false);
        break;
      case Command.DUMP_TREE:
        LogManager.logTreeDump();
        break;
      case Command.DECREASE_TTS_RATE:
        ChromeVox.tts.increaseOrDecreaseProperty(TtsSettings.RATE, false);
        return false;
      case Command.INCREASE_TTS_RATE:
        ChromeVox.tts.increaseOrDecreaseProperty(TtsSettings.RATE, true);
        return false;
      case Command.DECREASE_TTS_PITCH:
        ChromeVox.tts.increaseOrDecreaseProperty(TtsSettings.PITCH, false);
        return false;
      case Command.INCREASE_TTS_PITCH:
        ChromeVox.tts.increaseOrDecreaseProperty(TtsSettings.PITCH, true);
        return false;
      case Command.DECREASE_TTS_VOLUME:
        ChromeVox.tts.increaseOrDecreaseProperty(TtsSettings.VOLUME, false);
        return false;
      case Command.INCREASE_TTS_VOLUME:
        ChromeVox.tts.increaseOrDecreaseProperty(TtsSettings.VOLUME, true);
        return false;
      case Command.STOP_SPEECH:
        ChromeVox.tts.stop();
        // TODO(b/314203187): Not null asserted, check that this is correct.
        ChromeVoxState.instance!.isReadingContinuously = false;
        return false;
      case Command.TOGGLE_EARCONS:
        ChromeVox.earcons.toggle();
        return false;
      case Command.CYCLE_TYPING_ECHO:
        TypingEcho.cycleWithAnnouncement();
        return false;
      case Command.CYCLE_PUNCTUATION_ECHO:
        ChromeVox.tts.speak(
            Msgs.getMsg(TtsBackground.primary.cyclePunctuationEcho()),
            QueueMode.FLUSH);
        return false;
      case Command.REPORT_ISSUE:
        this.reportIssue_();
        return false;
      case Command.TOGGLE_BRAILLE_CAPTIONS:
        BrailleCaptionsBackground.setActive(
            !BrailleCaptionsBackground.isEnabled());
        return false;
      case Command.TOGGLE_BRAILLE_TABLE:
        BrailleTranslatorManager.instance.toggleBrailleTable();
        return false;
      case Command.HELP:
        (new PanelCommand(PanelCommandType.TUTORIAL)).send();
        return false;
      case Command.TOGGLE_SCREEN:
        this.toggleScreen_();
        return false;
      case Command.TOGGLE_SPEECH_ON_OR_OFF:
        TtsBackground.toggleSpeechWithAnnouncement();
        return false;
      case Command.ENABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP:
        this.enableChromeVoxArcSupportForCurrentApp_();
        break;
      case Command.DISABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP:
        this.disableChromeVoxArcSupportForCurrentApp_();
        break;
      case Command.SHOW_TALKBACK_KEYBOARD_SHORTCUTS:
        this.showTalkBackKeyboardShortcuts_();
        return false;
      case Command.SHOW_TTS_SETTINGS:
        chrome.accessibilityPrivate.openSettingsSubpage(
            'manageAccessibility/tts');
        break;
      default:
        break;
      case Command.TOGGLE_KEYBOARD_HELP:
        (new PanelCommand(PanelCommandType.OPEN_MENUS)).send();
        return false;
      case Command.SHOW_PANEL_MENU_MOST_RECENT:
        (new PanelCommand(PanelCommandType.OPEN_MENUS_MOST_RECENT)).send();
        return false;
      case Command.NEXT_GRANULARITY:
      case Command.PREVIOUS_GRANULARITY:
        this.nextOrPreviousGranularity_(
            command === Command.PREVIOUS_GRANULARITY);
        return false;
      case Command.ANNOUNCE_BATTERY_DESCRIPTION:
        this.announceBatteryDescription_();
        break;
      case Command.RESET_TEXT_TO_SPEECH_SETTINGS:
        TtsBackground.resetTextToSpeechSettings();
        return false;
      case Command.COPY:
        EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});

        // The above command doesn't trigger document clipboard events, so we
        // need to set this manually.
        ClipboardHandler.instance.readNextClipboardDataChange();
        return false;
      case Command.TOGGLE_DICTATION:
        EventGenerator.sendKeyPress(KeyCode.D, {search: true});
        return false;
      case Command.OPEN_KEYBOARD_SHORTCUTS:
        EventGenerator.sendKeyPress(KeyCode.S, {search: true, ctrl: true});
        return false;
    }

    // The remaining commands require a current range.
    if (!ChromeVoxRange.current) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      if (!ChromeVoxState.instance!.talkBackEnabled) {
        this.announceNoCurrentRange_();
      }
      return true;
    }

    // Allow edit commands first.
    if (!this.onEditCommand_(command)) {
      return false;
    }

    let currentRange: CursorRange | null = ChromeVoxRange.current;
    let node: AutomationNode | undefined = currentRange.start.node;

    // If true, will check if the predicate matches the current node.
    let matchCurrent = false;

    let dir = Dir.FORWARD;
    let newRangeData;
    let pred: AutomationPredicate.Unary | null = null;
    let predErrorMsg: string|undefined = undefined;
    let rootPred = AutomationPredicate.rootOrEditableRoot;
    let unit = null;
    let shouldWrap = true;
    const speechProps = new TtsSpeechProperties();
    let skipSync = false;
    let didNavigate = false;
    let tryScrolling = true;
    let skipSettingSelection = false;
    let skipInitialAncestry = true;
    switch (command) {
      case Command.NEXT_CHARACTER:
        didNavigate = true;
        speechProps.phoneticCharacters = true;
        unit = CursorUnit.CHARACTER;
        currentRange = currentRange.move(CursorUnit.CHARACTER, Dir.FORWARD);
        break;
      case Command.PREVIOUS_CHARACTER:
        dir = Dir.BACKWARD;
        didNavigate = true;
        speechProps.phoneticCharacters = true;
        unit = CursorUnit.CHARACTER;
        currentRange = currentRange.move(CursorUnit.CHARACTER, dir);
        break;
      case Command.NATIVE_NEXT_CHARACTER:
      case Command.NATIVE_PREVIOUS_CHARACTER:
        // TODO(b/314203187): Not null asserted, check that this is correct.
        DesktopAutomationInterface.instance!.onNativeNextOrPreviousCharacter();
        return true;
      case Command.NEXT_WORD:
        didNavigate = true;
        unit = CursorUnit.WORD;
        currentRange = currentRange.move(CursorUnit.WORD, Dir.FORWARD);
        break;
      case Command.PREVIOUS_WORD:
        dir = Dir.BACKWARD;
        didNavigate = true;
        unit = CursorUnit.WORD;
        currentRange = currentRange.move(CursorUnit.WORD, dir);
        break;
      case Command.NATIVE_NEXT_WORD:
      case Command.NATIVE_PREVIOUS_WORD:
        // TODO(b/314203187): Not null asserted, check that this is correct.
        DesktopAutomationInterface.instance!.onNativeNextOrPreviousWord(
            command === Command.NATIVE_NEXT_WORD);
        return true;
      case Command.FORWARD:
      case Command.NEXT_LINE:
        didNavigate = true;
        unit = CursorUnit.LINE;
        currentRange = currentRange.move(CursorUnit.LINE, Dir.FORWARD);
        break;
      case Command.BACKWARD:
      case Command.PREVIOUS_LINE:
        dir = Dir.BACKWARD;
        didNavigate = true;
        unit = CursorUnit.LINE;
        currentRange = currentRange.move(CursorUnit.LINE, dir);
        break;
      case Command.NEXT_BUTTON:
        dir = Dir.FORWARD;
        pred = AutomationPredicate.button;
        predErrorMsg = 'no_next_button';
        break;
      case Command.PREVIOUS_BUTTON:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.button;
        predErrorMsg = 'no_previous_button';
        break;
      case Command.NEXT_CHECKBOX:
        pred = AutomationPredicate.checkBox;
        predErrorMsg = 'no_next_checkbox';
        break;
      case Command.PREVIOUS_CHECKBOX:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.checkBox;
        predErrorMsg = 'no_previous_checkbox';
        break;
      case Command.NEXT_COMBO_BOX:
        pred = AutomationPredicate.comboBox;
        predErrorMsg = 'no_next_combo_box';
        break;
      case Command.PREVIOUS_COMBO_BOX:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.comboBox;
        predErrorMsg = 'no_previous_combo_box';
        break;
      case Command.NEXT_EDIT_TEXT:
        skipSettingSelection = true;
        pred = AutomationPredicate.editText;
        predErrorMsg = 'no_next_edit_text';
        // TODO(b/314203187): Not null asserted, check that this is correct.
        SmartStickyMode.instance!.startIgnoringRangeChanges();
        break;
      case Command.PREVIOUS_EDIT_TEXT:
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.editText;
        predErrorMsg = 'no_previous_edit_text';
        // TODO(b/314203187): Not null asserted, check that this is correct.
        SmartStickyMode.instance!.startIgnoringRangeChanges();
        break;
      case Command.NEXT_FORM_FIELD:
        skipSettingSelection = true;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_next_form_field';
        // TODO(b/314203187): Not null asserted, check that this is correct.
        SmartStickyMode.instance!.startIgnoringRangeChanges();
        break;
      case Command.PREVIOUS_FORM_FIELD:
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_previous_form_field';
        // TODO(b/314203187): Not null asserted, check that this is correct.
        SmartStickyMode.instance!.startIgnoringRangeChanges();
        break;
      case Command.PREVIOUS_GRAPHIC:
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.image;
        predErrorMsg = 'no_previous_graphic';
        break;
      case Command.NEXT_GRAPHIC:
        skipSettingSelection = true;
        pred = AutomationPredicate.image;
        predErrorMsg = 'no_next_graphic';
        break;
      case Command.NEXT_HEADING:
        pred = AutomationPredicate.heading;
        predErrorMsg = 'no_next_heading';
        break;
      case Command.NEXT_HEADING_1:
        pred = AutomationPredicate.makeHeadingPredicate(1);
        predErrorMsg = 'no_next_heading_1';
        break;
      case Command.NEXT_HEADING_2:
        pred = AutomationPredicate.makeHeadingPredicate(2);
        predErrorMsg = 'no_next_heading_2';
        break;
      case Command.NEXT_HEADING_3:
        pred = AutomationPredicate.makeHeadingPredicate(3);
        predErrorMsg = 'no_next_heading_3';
        break;
      case Command.NEXT_HEADING_4:
        pred = AutomationPredicate.makeHeadingPredicate(4);
        predErrorMsg = 'no_next_heading_4';
        break;
      case Command.NEXT_HEADING_5:
        pred = AutomationPredicate.makeHeadingPredicate(5);
        predErrorMsg = 'no_next_heading_5';
        break;
      case Command.NEXT_HEADING_6:
        pred = AutomationPredicate.makeHeadingPredicate(6);
        predErrorMsg = 'no_next_heading_6';
        break;
      case Command.PREVIOUS_HEADING:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.heading;
        predErrorMsg = 'no_previous_heading';
        break;
      case Command.PREVIOUS_HEADING_1:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(1);
        predErrorMsg = 'no_previous_heading_1';
        break;
      case Command.PREVIOUS_HEADING_2:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(2);
        predErrorMsg = 'no_previous_heading_2';
        break;
      case Command.PREVIOUS_HEADING_3:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(3);
        predErrorMsg = 'no_previous_heading_3';
        break;
      case Command.PREVIOUS_HEADING_4:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(4);
        predErrorMsg = 'no_previous_heading_4';
        break;
      case Command.PREVIOUS_HEADING_5:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(5);
        predErrorMsg = 'no_previous_heading_5';
        break;
      case Command.PREVIOUS_HEADING_6:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(6);
        predErrorMsg = 'no_previous_heading_6';
        break;
      case Command.NEXT_LINK:
        pred = AutomationPredicate.link;
        predErrorMsg = 'no_next_link';
        break;
      case Command.PREVIOUS_LINK:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.link;
        predErrorMsg = 'no_previous_link';
        break;
      case Command.NEXT_TABLE:
        pred = AutomationPredicate.table;
        predErrorMsg = 'no_next_table';
        break;
      case Command.PREVIOUS_TABLE:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.table;
        predErrorMsg = 'no_previous_table';
        break;
      case Command.NEXT_VISITED_LINK:
        pred = AutomationPredicate.visitedLink;
        predErrorMsg = 'no_next_visited_link';
        break;
      case Command.PREVIOUS_VISITED_LINK:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.visitedLink;
        predErrorMsg = 'no_previous_visited_link';
        break;
      case Command.NEXT_LANDMARK:
        pred = AutomationPredicate.landmark;
        predErrorMsg = 'no_next_landmark';
        break;
      case Command.PREVIOUS_LANDMARK:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.landmark;
        predErrorMsg = 'no_previous_landmark';
        break;
      // falls through.
      case Command.LEFT:
      // @ts-expect-error Fallthrough is disabled by TypeScript.
      case Command.PREVIOUS_OBJECT:
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        // falls through.
      case Command.RIGHT:
      case Command.NEXT_OBJECT:
        skipSettingSelection = true;
        didNavigate = true;
        unit = (EventSource.get() === EventSourceType.TOUCH_GESTURE) ?
            CursorUnit.GESTURE_NODE :
            CursorUnit.NODE;
        currentRange = currentRange.move(unit, dir);
        currentRange = this.skipLabelOrDescriptionFor(currentRange, dir);
        break;
      case Command.PREVIOUS_GROUP:
        skipSync = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.group;
        break;
      case Command.NEXT_GROUP:
        skipSync = true;
        pred = AutomationPredicate.group;
        break;
      case Command.PREVIOUS_PAGE:
      case Command.NEXT_PAGE:
        this.nextOrPreviousPage_(command, currentRange);
        return false;
      case Command.PREVIOUS_SIMILAR_ITEM:
        dir = Dir.BACKWARD;
        skipSync = true;
        pred = this.getPredicateForNextOrPreviousSimilarItem_(node);
        break;
      case Command.NEXT_SIMILAR_ITEM:
        skipSync = true;
        pred = this.getPredicateForNextOrPreviousSimilarItem_(node);
        break;
      case Command.PREVIOUS_INVALID_ITEM:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.isInvalid;
        rootPred = AutomationPredicate.root;
        predErrorMsg = 'no_invalid_item';
        break;
      case Command.NEXT_INVALID_ITEM:
        pred = AutomationPredicate.isInvalid;
        rootPred = AutomationPredicate.root;
        predErrorMsg = 'no_invalid_item';
        break;
      case Command.NEXT_LIST:
        pred = AutomationPredicate.makeListPredicate(node);
        predErrorMsg = 'no_next_list';
        break;
      case Command.PREVIOUS_LIST:
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeListPredicate(node);
        predErrorMsg = 'no_previous_list';
        skipInitialAncestry = false;
        break;
      case Command.JUMP_TO_TOP:
        newRangeData = this.getNewRangeForJumpToTop_(node, currentRange);
        currentRange = newRangeData.range;
        tryScrolling = false;
        break;
      case Command.JUMP_TO_BOTTOM:
        newRangeData = this.getNewRangeForJumpToBottom_(node, currentRange);
        currentRange = newRangeData.range;
        tryScrolling = false;
        break;
      case Command.FORCE_CLICK_ON_CURRENT_ITEM:
        this.forceClickOnCurrentItem_();
        // Skip all other processing; if focus changes, we should get an event
        // for that.
        return false;
      case Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM:
        node.longClick();
        // Skip all other processing; if focus changes, we should get an event
        // for that.
        return false;
      case Command.JUMP_TO_DETAILS:
        newRangeData = this.getNewRangeForJumpToDetails_(node, currentRange);
        node = newRangeData.node;
        currentRange = newRangeData.range;
        break;
      case Command.READ_FROM_HERE:
        this.readFromHere_();
        return false;
      case Command.CONTEXT_MENU:
        EventGenerator.sendKeyPress(KeyCode.APPS);
        break;
      case Command.SHOW_HEADINGS_LIST:
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_heading')).send();
        return false;
      case Command.SHOW_FORMS_LIST:
        (new PanelCommand(
             PanelCommandType.OPEN_MENUS, 'panel_menu_form_controls'))
            .send();
        return false;
      case Command.SHOW_LANDMARKS_LIST:
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_landmark')).send();
        return false;
      case Command.SHOW_LINKS_LIST:
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_link')).send();
        return false;
      case Command.SHOW_ACTIONS_MENU:
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'panel_menu_actions'))
            .send();
        return false;
      case Command.SHOW_TABLES_LIST:
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'table_strategy'))
            .send();
        return false;
      case Command.TOGGLE_SEARCH_WIDGET:
        (new PanelCommand(PanelCommandType.SEARCH)).send();
        return false;
      case Command.READ_CURRENT_TITLE:
        this.readCurrentTitle_();
        return false;
      case Command.READ_CURRENT_URL:
        // TODO(b/314203187): Not null asserted, check that this is correct.
        new Output().withString(node.root!.docUrl || '').go();
        return false;
      case Command.TOGGLE_SELECTION:
        // If the selection was toggled off, return.
        if (!ChromeVoxRange.toggleSelection()) {
          return false;
        }
        break;
      case Command.FULLY_DESCRIBE:
        const o = new Output();
        o.withContextFirst()
            .withRichSpeechAndBraille(
                currentRange, undefined, OutputCustomEvent.NAVIGATE)
            .go();
        return false;
      case Command.VIEW_GRAPHIC_AS_BRAILLE:
        this.viewGraphicAsBraille_(currentRange);
        return false;
      // Table commands.
      case Command.PREVIOUS_ROW:
        skipSync = true;
        dir = Dir.BACKWARD;
        pred = this.getPredicateForPreviousRow_(currentRange, dir);
        predErrorMsg = 'no_cell_above';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
        break;
      case Command.PREVIOUS_COL:
        skipSync = true;
        dir = Dir.BACKWARD;
        pred = this.getPredicateForPreviousCol_(currentRange, dir);
        predErrorMsg = 'no_cell_left';
        rootPred = AutomationPredicate.row;
        shouldWrap = false;
        break;
      case Command.NEXT_ROW:
        skipSync = true;
        pred = this.getPredicateForNextRow_(currentRange, dir);
        predErrorMsg = 'no_cell_below';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
        break;
      case Command.NEXT_COL:
        skipSync = true;
        pred = this.getPredicateForNextCol_(currentRange, dir);
        predErrorMsg = 'no_cell_right';
        rootPred = AutomationPredicate.row;
        shouldWrap = false;
        break;
      case Command.GO_TO_ROW_FIRST_CELL:
      case Command.GO_TO_ROW_LAST_CELL:
        skipSync = true;
        newRangeData = this.getNewRangeForGoToRowFirstOrLastCell_(
            node, currentRange, command);
        node = newRangeData.node;
        currentRange = newRangeData.range;
        break;
      case Command.GO_TO_COL_FIRST_CELL:
        skipSync = true;
        node = this.getTableNode_(node);
        if (!node || !node.firstChild) {
          return false;
        }
        pred = this.getPredicateForGoToColFirstOrLastCell_(currentRange, dir);
        currentRange = CursorRange.fromNode(node.firstChild);
        // Should not be outputted.
        predErrorMsg = 'no_cell_above';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
        break;
      case Command.GO_TO_COL_LAST_CELL:
        skipSync = true;
        dir = Dir.BACKWARD;
        node = this.getTableNode_(node);
        if (!node || !node.lastChild) {
          return false;
        }
        pred = this.getPredicateForGoToColFirstOrLastCell_(currentRange, dir);

        newRangeData = this.getNewRangeForGoToColLastCell_(node);
        currentRange = newRangeData.range;
        matchCurrent = true;

        // Should not be outputted.
        predErrorMsg = 'no_cell_below';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
        break;
      case Command.GO_TO_FIRST_CELL:
      case Command.GO_TO_LAST_CELL:
        skipSync = true;
        node = this.getTableNode_(node);
        if (!node) {
          break;
        }
        newRangeData = this.getNewRangeForGoToFirstOrLastCell_(
            node, currentRange, command);
        currentRange = newRangeData.range;
        break;

      // These commands are only available when invoked from touch.
      case Command.NEXT_AT_GRANULARITY:
      case Command.PREVIOUS_AT_GRANULARITY:
        this.nextOrPreviousAtGranularity_(
            command === Command.PREVIOUS_AT_GRANULARITY);
        return false;
      case Command.ANNOUNCE_RICH_TEXT_DESCRIPTION:
        this.announceRichTextDescription_(node);
        return false;
      case Command.READ_PHONETIC_PRONUNCIATION:
        this.readPhoneticPronunciation_(node);
        return false;
      case Command.READ_LINK_URL:
        this.readLinkUrl_(node);
        return false;
      default:
        return true;
    }

    if (didNavigate) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.ChromeVox.Navigate');
    }

    // TODO(accessibility): extract this block and remove explicit type casts
    // after re-writing.
    if (pred) {
      chrome.metricsPrivate.recordUserAction('Accessibility.ChromeVox.Jump');

      // TODO(b/314203187): Not null asserted, check that this is correct.
      let bound = currentRange!.getBound(dir).node;
      if (bound) {
        let node = null;

        if (matchCurrent && pred(bound)) {
          node = bound;
        }

        if (!node) {
          node = AutomationUtil.findNextNode(
              bound, dir, pred, {skipInitialAncestry, root: rootPred});
        }

        // Scroll here for table navigation with arrow keys where some nodes may
        // be hidden. The scroll must happen here because |node| will remain
        // undefined, causing this command to return before the next autoscroll
        // check.
        // TODO(b/314203187): Not null asserted, check that this is correct.
        if (!node &&
            !AutoScrollHandler.instance!.scrollToFindNodes(
                bound, command, currentRange!, dir, () => {
                  this.onCommand(command);
                  this.onFinishCommand();
                })) {
          this.onFinishCommand();
          return false;
        }

        if (node && !skipSync) {
          node = AutomationUtil.findNodePre(
                     node, Dir.FORWARD, AutomationPredicate.object) ??
              node;
        }

        if (node) {
          currentRange = CursorRange.fromNode(node);
        } else {
          ChromeVox.earcons.playEarcon(EarconId.WRAP);
          if (!shouldWrap) {
            if (predErrorMsg) {
              new Output()
                  .withString(Msgs.getMsg(predErrorMsg))
                  .withQueueMode(QueueMode.FLUSH)
                  .go();
            }
            this.onFinishCommand();
            return false;
          }

          let root: AutomationNode | undefined = bound;
          while (root && !AutomationPredicate.rootOrEditableRoot(root)) {
            root = root.parent;
          }

          if (!root) {
            root = bound.root;
          }

          if (dir === Dir.FORWARD) {
            // TODO(b/314203187): Not null asserted, check that this is correct.
            bound = root!;
          } else {
            bound =
                AutomationUtil.findNodePost(
                    root as AutomationNode, dir, AutomationPredicate.leaf) ??
                bound;
          }
          node = AutomationUtil.findNextNode(
              bound as AutomationNode, dir, pred, {root: rootPred});

          if (node && !skipSync) {
            node = AutomationUtil.findNodePre(
                       node, Dir.FORWARD, AutomationPredicate.object) ??
                node;
          }

          if (node) {
            currentRange = CursorRange.fromNode(node);
          } else if (predErrorMsg) {
            new Output()
                .withString(Msgs.getMsg(predErrorMsg))
                .withQueueMode(QueueMode.FLUSH)
                .go();
            this.onFinishCommand();
            return false;
          }
        }
      }
    }

    // TODO(accessibility): extract into function.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (tryScrolling && currentRange &&
        !AutoScrollHandler.instance!.onCommandNavigation(
            currentRange, dir, pred, unit, speechProps, rootPred, () => {
              this.onCommand(command);
              this.onFinishCommand();
            })) {
      this.onFinishCommand();
      return false;
    }

    if (currentRange) {
      if (currentRange.wrapped) {
        ChromeVox.earcons.playEarcon(EarconId.WRAP);
      }

      ChromeVoxRange.navigateTo(
          currentRange, undefined, speechProps, skipSettingSelection);
    }

    this.onFinishCommand();
    return false;
  }

  /** Finishes processing of a command. */
  onFinishCommand(): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    SmartStickyMode.instance!.stopIgnoringRangeChanges();
  }

  /**
   * Handle the command to view the first graphic within the current range
   * as braille.
   */
  private viewGraphicAsBraille_(currentRange: CursorRange): void {
    // Find the first node within the current range that supports image data.
    const imageNode = AutomationUtil.findNodePost(
        currentRange.start.node, Dir.FORWARD,
        AutomationPredicate.supportsImageData);
    if (imageNode) {
      imageNode.getImageData(0, 0);
    }
  }

  /**
   * Provides a partial mapping from ChromeVox key combinations to
   * Search-as-a-function key as seen in Chrome OS documentation.
   * @return True if the command should propagate.
   */
  private onEditCommand_(command: Command): boolean {
    if (ChromeVoxPrefs.isStickyModeOn()) {
      return true;
    }
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const textEditHandler =
        DesktopAutomationInterface.instance!.textEditHandler;
    if (!textEditHandler ||
        !AutomationUtil.isDescendantOf(
            ChromeVoxRange.current!.start.node, textEditHandler.node)) {
      return true;
    }

    // Skip customized keys for read only text fields.
    if (textEditHandler.node.restriction === Restriction.READ_ONLY) {
      return true;
    }

    // Skips customized keys if they get suppressed in speech.
    if (AutomationPredicate.shouldOnlyOutputSelectionChangeInBraille(
            textEditHandler.node)) {
      return true;
    }

    const isMultiline = AutomationPredicate.multiline(textEditHandler.node);
    switch (command) {
      case Command.PREVIOUS_CHARACTER:
        EventGenerator.sendKeyPress(KeyCode.HOME, {shift: true});
        break;
      case Command.NEXT_CHARACTER:
        EventGenerator.sendKeyPress(KeyCode.END, {shift: true});
        break;
      case Command.PREVIOUS_WORD:
        EventGenerator.sendKeyPress(KeyCode.HOME, {shift: true, ctrl: true});
        break;
      case Command.NEXT_WORD:
        EventGenerator.sendKeyPress(KeyCode.END, {shift: true, ctrl: true});
        break;
      case Command.PREVIOUS_OBJECT:
        if (!isMultiline) {
          return true;
        }

        if (textEditHandler.isSelectionOnFirstLine()) {
          ChromeVoxRange.set(CursorRange.fromNode(textEditHandler.node));
          return true;
        }
        EventGenerator.sendKeyPress(KeyCode.HOME);
        break;
      case Command.NEXT_OBJECT:
        if (!isMultiline) {
          return true;
        }

        if (textEditHandler.isSelectionOnLastLine()) {
          textEditHandler.moveToAfterEditText();
          return false;
        }

        EventGenerator.sendKeyPress(KeyCode.END);
        break;
      case Command.PREVIOUS_LINE:
        if (!isMultiline) {
          return true;
        }
        if (textEditHandler.isSelectionOnFirstLine()) {
          ChromeVoxRange.set(CursorRange.fromNode(textEditHandler.node));
          return true;
        }
        EventGenerator.sendKeyPress(KeyCode.PRIOR);
        break;
      case Command.NEXT_LINE:
        if (!isMultiline) {
          return true;
        }

        if (textEditHandler.isSelectionOnLastLine()) {
          textEditHandler.moveToAfterEditText();
          return false;
        }
        EventGenerator.sendKeyPress(KeyCode.NEXT);
        break;
      case Command.JUMP_TO_TOP:
        EventGenerator.sendKeyPress(KeyCode.HOME, {ctrl: true});
        break;
      case Command.JUMP_TO_BOTTOM:
        EventGenerator.sendKeyPress(KeyCode.END, {ctrl: true});
        break;
      default:
        return true;
    }
    return false;
  }

  skipLabelOrDescriptionFor(currentRange: CursorRange, dir: Dir):
      CursorRange | null {
    if (!currentRange) {
      return null;
    }

    // Keep moving past all nodes acting as labels or descriptions.
    while (currentRange?.start?.node?.role === RoleType.STATIC_TEXT) {
      // We must scan upwards as any ancestor might have a label or description.
      let ancestor: AutomationNode | undefined = currentRange.start.node;
      while (ancestor) {
        if (ancestor.labelFor?.length || ancestor.descriptionFor?.length) {
          break;
        }
        ancestor = ancestor.parent;
      }
      if (ancestor) {
        currentRange = currentRange.move(CursorUnit.NODE, dir);
      } else {
        break;
      }
    }

    return currentRange;
  }

  private announceBatteryDescription_(): void {
    chrome.accessibilityPrivate.getBatteryDescription(batteryDescription => {
      new Output()
          .withString(batteryDescription)
          .withQueueMode(QueueMode.FLUSH)
          .go();
    });
  }

  private announceNoCurrentRange_(): void {
    new Output()
        .withString(Msgs.getMsg(
            EventSource.get() === EventSourceType.TOUCH_GESTURE ?
                'no_focus_touch' :
                'no_focus'))
        .withQueueMode(QueueMode.FLUSH)
        .go();
  }

  private announceRichTextDescription_(node: AutomationNode): void {
    const optSubs = [];
    node.fontSize ? optSubs.push('font size: ' + node.fontSize) :
                    optSubs.push('');
    node.color ? optSubs.push(Color.getColorDescription(node.color)) :
                 optSubs.push('');
    node.bold ? optSubs.push(Msgs.getMsg('bold')) : optSubs.push('');
    node.italic ? optSubs.push(Msgs.getMsg('italic')) : optSubs.push('');
    node.underline ? optSubs.push(Msgs.getMsg('underline')) : optSubs.push('');
    node.lineThrough ? optSubs.push(Msgs.getMsg('linethrough')) :
                       optSubs.push('');
    node.fontFamily ? optSubs.push('font family: ' + node.fontFamily) :
                      optSubs.push('');

    const richTextDescription = Msgs.getMsg('rich_text_attributes', optSubs);
    new Output()
        .withString(richTextDescription)
        .withQueueMode(QueueMode.CATEGORY_FLUSH)
        .go();
  }

  private disableChromeVoxArcSupportForCurrentApp_(): void {
    chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
        false, response => {
          if (response === SetNativeChromeVoxResponse.TALKBACK_NOT_INSTALLED) {
            ChromeVox.braille.write(
                NavBraille.fromText(Msgs.getMsg('announce_install_talkback')));
            ChromeVox.tts.speak(
                Msgs.getMsg('announce_install_talkback'), QueueMode.FLUSH);
          } else if (
              response ===
              SetNativeChromeVoxResponse.NEED_DEPRECATION_CONFIRMATION) {
            ChromeVox.braille.write(NavBraille.fromText(
                Msgs.getMsg('announce_talkback_deprecation')));
            ChromeVox.tts.speak(
                Msgs.getMsg('announce_talkback_deprecation'), QueueMode.FLUSH);
          }
        });
  }

  private enableChromeVoxArcSupportForCurrentApp_(): void {
    chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
        true, _response => {});
  }

  private forceClickOnCurrentItem_(): void {
    if (!ChromeVoxRange.current) {
      return;
    }
    let actionNode: AutomationNode | undefined =
        ChromeVoxRange.current.start.node;
    // Scan for a clickable, which overrides the |actionNode|.
    let clickableNode: AutomationNode | undefined = actionNode;
    while (!clickableNode?.clickable &&
           actionNode.root === clickableNode?.root) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      clickableNode = clickableNode!.parent;
    }
    if (actionNode.root === clickableNode?.root) {
      clickableNode?.doDefault();
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (EventSource.get() === EventSourceType.TOUCH_GESTURE &&
        actionNode.state![StateType.EDITABLE]) {
      // Dispatch a click to ensure the VK gets shown.
      const center = RectUtil.center(actionNode.location);
      EventGenerator.sendMouseClick(center.x, center.y);
      return;
    }

    while (actionNode && AutomationPredicate.text(actionNode)) {
      actionNode = actionNode.parent;
    }
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (actionNode!.inPageLinkTarget) {
      ChromeVoxRange.navigateTo(
          CursorRange.fromNode(actionNode!.inPageLinkTarget));
      return;
    }
    actionNode!.doDefault();
  }

  private getNewRangeForGoToColLastCell_(node: AutomationNode): NewRangeData {
    // Try to start on the last cell of the table and allow
    // matching that node.
    let startNode = node.lastChild;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    while (startNode?.lastChild && !AutomationPredicate.cellLike(startNode!)) {
      startNode = startNode.lastChild;
    }
    return {node: startNode, range: CursorRange.fromNode(startNode!)};
  }

  private getNewRangeForGoToFirstOrLastCell_(
      node: AutomationNode, currentRange: CursorRange, command: Command)
      : NewRangeData {
    const end = AutomationUtil.findNodePost(
        node, command === Command.GO_TO_LAST_CELL ? Dir.BACKWARD : Dir.FORWARD,
        AutomationPredicate.leaf);
    if (end) {
      return {node: end, range: CursorRange.fromNode(end)};
    }
    return {node, range: currentRange};
  }

  private getNewRangeForJumpToBottom_(
      node: AutomationNode, currentRange: CursorRange): NewRangeData {
    if (!currentRange.start.node || !currentRange.start.node.root) {
      return {node, range: currentRange};
    }
    const newNode = AutomationUtil.findLastNode(
        currentRange.start.node.root, AutomationPredicate.object);
    if (newNode) {
      return {node: newNode, range: CursorRange.fromNode(newNode)};
    }
    return {node, range: currentRange};
  }

  private getNewRangeForJumpToTop_(
      node: AutomationNode, currentRange: CursorRange): NewRangeData {
    const root = currentRange.start.node?.root;
    if (!root) {
      return {node, range: currentRange};
    }
    const newNode = AutomationUtil.findNodePost(
        root, Dir.FORWARD, AutomationPredicate.object);
    if (newNode) {
      return {node: newNode, range: CursorRange.fromNode(newNode)};
    }
    return {node, range: currentRange};
  }

  private getNewRangeForGoToRowFirstOrLastCell_(
      node: AutomationNode, currentRange: CursorRange, command: Command)
      : NewRangeData {
    let current: AutomationNode | undefined = node;
    while (current && current.role !== RoleType.ROW) {
      current = current.parent;
    }
    if (!current) {
      return {node: current, range: currentRange};
    }
    const end = AutomationUtil.findNodePost(
        current,
        command === Command.GO_TO_ROW_LAST_CELL ? Dir.BACKWARD : Dir.FORWARD,
        AutomationPredicate.leaf);
    if (end) {
      currentRange = CursorRange.fromNode(end);
    }
    return {node: current, range: currentRange};
  }

  private getNewRangeForJumpToDetails_(
      node: AutomationNode, currentRange: CursorRange): NewRangeData {
    let current: AutomationNode | undefined = node;
    while (current && !current.details) {
      current = current.parent;
    }
    if (current?.details?.length) {
      // TODO currently can only jump to first detail.
      currentRange = CursorRange.fromNode(current.details[0]);
    }
    return {node: current, range: currentRange};
  }

  private getPredicateForGoToColFirstOrLastCell_(
      currentRange: CursorRange, dir: Dir): AutomationPredicate.Unary | null {
    const tableOpts = {col: true, dir, end: true};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  private getPredicateForNextCol_(
      currentRange: CursorRange, dir: Dir): AutomationPredicate.Unary | null {
    const tableOpts = {col: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  private getPredicateForNextRow_(
      currentRange: CursorRange, dir: Dir): AutomationPredicate.Unary | null {
    const tableOpts = {row: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  private getPredicateForNextOrPreviousSimilarItem_(
      node: AutomationNode): AutomationPredicate.Unary | null {
    const originalNode = node;
    let current: AutomationNode | undefined = node;

    // Scan upwards until we get a role we don't want to ignore.
    while (current && AutomationPredicate.ignoreDuringJump(current)) {
      current = current.parent;
    }

    const useNode = current ?? originalNode;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return AutomationPredicate.roles([useNode.role!]);
  }

  private getPredicateForPreviousCol_(
      currentRange: CursorRange, dir: Dir): AutomationPredicate.Unary | null {
    const tableOpts = {col: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  private getPredicateForPreviousRow_(
      currentRange: CursorRange, dir: Dir): AutomationPredicate.Unary | null {
    const tableOpts = {row: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  private getTableNode_(node: AutomationNode): AutomationNode | undefined {
    let current: AutomationNode | undefined = node;
    while (current && current.role !== RoleType.TABLE) {
      current = current.parent;
    }
    return current;
  }

  private nextOrPreviousAtGranularity_(isPrevious: boolean): void {
    let command;
    switch (GestureInterface.getGranularity()) {
      case GestureGranularity.CHARACTER:
        command =
            isPrevious ? Command.PREVIOUS_CHARACTER : Command.NEXT_CHARACTER;
        break;
      case GestureGranularity.WORD:
        command = isPrevious ? Command.PREVIOUS_WORD : Command.NEXT_WORD;
        break;
      case GestureGranularity.LINE:
        command = isPrevious ? Command.PREVIOUS_LINE : Command.NEXT_LINE;
        break;
      case GestureGranularity.HEADING:
        command = isPrevious ? Command.PREVIOUS_HEADING : Command.NEXT_HEADING;
        break;
      case GestureGranularity.LINK:
        command = isPrevious ? Command.PREVIOUS_LINK : Command.NEXT_LINK;
        break;
      case GestureGranularity.FORM_FIELD_CONTROL:
        command =
            isPrevious ? Command.PREVIOUS_FORM_FIELD : Command.NEXT_FORM_FIELD;
        break;
    }
    if (command) {
      this.onCommand(command);
    }
  }

  private nextOrPreviousPage_(
      command: Command, currentRange: CursorRange): void {
    const root = AutomationUtil.getTopLevelRoot(currentRange.start.node);
    if (root?.scrollY !== undefined) {
      let page = Math.ceil(root.scrollY / root.location.height) || 1;
      page = command === Command.NEXT_PAGE ? page + 1 : page - 1;
      ChromeVox.tts.stop();
      root.setScrollOffset(0, page * root.location.height);
    }
  }

  private readCurrentTitle_(): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    let target: AutomationNode | undefined = ChromeVoxRange.current!.start.node;
    const output = new Output();

    if (!target) {
      return;
    }

    let firstWindow;
    let rootViewWindow;
    if (target.root?.role === RoleType.DESKTOP) {
      // Search for the first container with a name.
      while (target && (!target.name || !AutomationPredicate.root(target))) {
        target = target.parent;
      }
    } else {
      // Search for a root window with a title.
      while (target) {
        const isNamedWindow =
            Boolean(target.name) && target.role === RoleType.WINDOW;
        const isRootView = target.className === 'RootView';
        if (isNamedWindow && !firstWindow) {
          firstWindow = target;
        }

        if (isNamedWindow && isRootView) {
          rootViewWindow = target;
          break;
        }
        target = target.parent;
      }
    }

    // Re-target with preference for the root.
    target = rootViewWindow ?? firstWindow ?? target;

    if (!target) {
      output.format('@no_title');
    } else if (target.name) {
      output.withString(target.name);
    }

    output.go();
  }

  private readFromHere_(): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    ChromeVoxState.instance!.isReadingContinuously = true;
    const continueReading = (): void => {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      if (!ChromeVoxState.instance!.isReadingContinuously ||
          !ChromeVoxRange.current) {
        return;
      }

      const prevRange = ChromeVoxRange.current;
      const newRange =
          ChromeVoxRange.current.move(CursorUnit.NODE, Dir.FORWARD);

      // Stop if we've wrapped back to the document.
      const maybeDoc = newRange.start.node;
      if (AutomationPredicate.root(maybeDoc)) {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        ChromeVoxState.instance!.isReadingContinuously = false;
        return;
      }

      ChromeVoxRange.set(newRange);
      newRange.select();

      const o =
          new Output()
              .withoutHints()
              .withRichSpeechAndBraille(
                  ChromeVoxRange.current, prevRange, OutputCustomEvent.NAVIGATE)
              .onSpeechEnd(continueReading);

      if (!o.hasSpeech) {
        continueReading();
        return;
      }

      o.go();
    };

    {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      const startNode = ChromeVoxRange.current!.start.node;
      const collapsedRange = CursorRange.fromNode(startNode);
      const o =
          new Output()
              .withoutHints()
              .withRichSpeechAndBraille(
                  collapsedRange, collapsedRange, OutputCustomEvent.NAVIGATE)
              .onSpeechEnd(continueReading);

      if (o.hasSpeech) {
        o.go();
      } else {
        continueReading();
      }
    }
  }

  private readLinkUrl_(node: AutomationNode): void {
    const rootNode = node.root;
    let current: AutomationNode | undefined = node;
    while (current && !current.url) {
      // URL could be an ancestor of current range.
      current = current.parent;
    }
    // Announce node's URL if it's not the root node; we don't want to
    // announce the URL of the current page.
    const url = (current && current !== rootNode) ? current.url : '';
    new Output()
        .withString(
            url ? Msgs.getMsg('url_behind_link', [url]) :
                  Msgs.getMsg('no_url_found'))
        .withQueueMode(QueueMode.CATEGORY_FLUSH)
        .go();
  }

  private readPhoneticPronunciation_(node: AutomationNode): void {
    // Get node info.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const index = ChromeVoxRange.current!.start.index;
    const name = node.name;
    // If there is no text to speak, inform the user and return early.
    if (!name) {
      new Output()
          .withString(Msgs.getMsg('empty_name'))
          .withQueueMode(QueueMode.CATEGORY_FLUSH)
          .go();
      return;
    }

    // Get word start and end indices.
    let wordStarts: number[];
    let wordEnds: number[];
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (node.role === RoleType.INLINE_TEXT_BOX) {
      wordStarts = node.wordStarts!;
      wordEnds = node.wordEnds!;
    } else {
      wordStarts = node.nonInlineTextWordStarts!;
      wordEnds = node.nonInlineTextWordEnds!;
    }
    // Find the word we want to speak phonetically. If index === -1, then
    // the index represents an entire node.
    let text = '';
    if (index === -1) {
      text = name;
    } else {
      for (let z = 0; z < wordStarts.length; ++z) {
        if (wordStarts[z] <= index && wordEnds[z] > index) {
          text = name.substring(wordStarts[z], wordEnds[z]);
          break;
        }
      }
    }

    const language = chrome.i18n.getUILanguage();
    const phoneticText = PhoneticData.forText(text, language);
    if (phoneticText) {
      new Output()
          .withString(phoneticText)
          .withQueueMode(QueueMode.CATEGORY_FLUSH)
          .go();
    }
  }

  private reportIssue_(): void {
    let url =
        'https://issuetracker.google.com/issues/new?component=1272895&type=BUG' +
        '&priority=P2&severity=S2&description=';
    const description: {[key: string]: string} = {};
    description['Chrome OS Version'] = chrome.runtime.getManifest()['version'];
    description['Lacros Version (if applicable)'] =
        '(copy from chrome://version)';
    description['Reproduction Steps'] = '%0a1.%0a2.%0a3.';
    description['Expected result'] = '';
    description['What actually happens'] = '';
    for (const key in description) {
      url += key + ':%20' + description[key] + '%0a';
    }
    BrowserUtil.openBrowserUrl(url);
  }

  private showLearnModePage_(): void {
    const explorerPage = {
      url: 'chromevox/learn_mode/learn_mode.html',
      type: 'panel' as CreateType,
    };
    // Use chrome.windows API to ensure page is opened in Ash-chrome.
    chrome.windows.create(explorerPage);
  }

  private showTalkBackKeyboardShortcuts_(): void {
    BrowserUtil.openBrowserUrl(
        'https://support.google.com/accessibility/android/answer/6110948');
  }

  private speakTimeAndDate_(): void {
    chrome.automation.getDesktop(d => {
      // First, try speaking the on-screen time.
      const allTime = d.findAll({role: RoleType.TIME});
      // TODO(b/314203187): Not null asserted, check that this is correct.
      allTime.filter(time => time.root!.role === RoleType.DESKTOP);

      let timeString = '';
      allTime.forEach(time => {
        if (time.name) {
          timeString = time.name;
        }
      });
      if (timeString) {
        ChromeVox.tts.speak(timeString, QueueMode.FLUSH);
        ChromeVox.braille.write(NavBraille.fromText(timeString));
      } else {
        // Fallback to the old way of speaking time.
        const output = new Output();
        const dateTime = new Date();
        output
            .withString(
                dateTime.toLocaleTimeString() + ', ' +
                dateTime.toLocaleDateString())
            .go();
      }
    });
  }

  /**
   * Launch ChromeVox options page in settings app.
   * TODO(b/268196299): Add test for showing options page.
   */
  private showOptionsPage_(): void {
    // Launch ChromeVox settings (inside ChromeOS Settings App).
    chrome.accessibilityPrivate.openSettingsSubpage('textToSpeech/chromeVox');
  }

  private nextOrPreviousGranularity_(isPrevious: boolean): void {
    let gran = GestureInterface.getGranularity();
    const next = isPrevious ?
        (--gran >= 0 ? gran : GestureGranularity.COUNT - 1) :
        ++gran % GestureGranularity.COUNT;
    GestureInterface.setGranularity(
        /** @type {GestureGranularity} */ (next));

    let announce = '';
    switch (GestureInterface.getGranularity()) {
      case GestureGranularity.CHARACTER:
        announce = Msgs.getMsg('character_granularity');
        break;
      case GestureGranularity.WORD:
        announce = Msgs.getMsg('word_granularity');
        break;
      case GestureGranularity.LINE:
        announce = Msgs.getMsg('line_granularity');
        break;
      case GestureGranularity.HEADING:
        announce = Msgs.getMsg('heading_granularity');
        break;
      case GestureGranularity.LINK:
        announce = Msgs.getMsg('link_granularity');
        break;
      case GestureGranularity.FORM_FIELD_CONTROL:
        announce = Msgs.getMsg('form_field_control_granularity');
        break;
    }
    ChromeVox.tts.speak(announce, QueueMode.FLUSH);
  }

  private toggleScreen_(): void {
    const newState = !ChromeVoxPrefs.darkScreen;
    if (newState && !LocalStorage.get('acceptToggleScreen')) {
      // If this is the first time, show a confirmation dialog.
      chrome.accessibilityPrivate.showConfirmationDialog(
          Msgs.getMsg('toggle_screen_title'),
          Msgs.getMsg('toggle_screen_description'), /*cancelName=*/ undefined,
          confirmed => {
            if (confirmed) {
              ChromeVoxPrefs.darkScreen = true;
              LocalStorage.set('acceptToggleScreen', true);
              chrome.accessibilityPrivate.darkenScreen(true);
              new Output().format('@toggle_screen_off').go();
            }
          });
    } else {
      ChromeVoxPrefs.darkScreen = newState;
      chrome.accessibilityPrivate.darkenScreen(newState);
      new Output()
          .format((newState) ? '@toggle_screen_off' : '@toggle_screen_on')
          .go();
    }
  }

  /** Performs global initialization. */
  static init(): void {
    CommandHandlerInterface.instance = new CommandHandler();
    ChromeVoxKbHandler.commandHandler = command =>
        CommandHandlerInterface.instance.onCommand(command);

    BridgeHelper.registerHandler(
        BridgeConstants.CommandHandler.TARGET,
        BridgeConstants.CommandHandler.Action.ON_COMMAND,
        (command: Command) => {
          if (Object.values(Command).includes(command)) {
            CommandHandlerInterface.instance.onCommand(command);
          } else {
            console.warn('ChromeVox got an unrecognized command: ' + command);
          }
        });
  }
}
