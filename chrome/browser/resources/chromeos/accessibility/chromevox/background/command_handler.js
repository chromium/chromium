// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox commands.
 */
import {AutomationPredicate} from '../../common/automation_predicate.js';
import {AutomationUtil} from '../../common/automation_util.js';
import {constants} from '../../common/constants.js';
import {Cursor, CursorUnit} from '../../common/cursors/cursor.js';
import {CursorRange} from '../../common/cursors/range.js';
import {EventGenerator} from '../../common/event_generator.js';
import {KeyCode} from '../../common/key_code.js';
import {LocalStorage} from '../../common/local_storage.js';
import {RectUtil} from '../../common/rect_util.js';
import {Earcon} from '../common/abstract_earcons.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {Command, CommandStore} from '../common/command_store.js';
import {ChromeVoxEvent, CustomAutomationEvent} from '../common/custom_automation_event.js';
import {EventSourceType} from '../common/event_source_type.js';
import {GestureGranularity} from '../common/gesture_command_data.js';
import {ChromeVoxKbHandler} from '../common/keyboard_handler.js';
import {LogType} from '../common/log_types.js';
import {Msgs} from '../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {TreeDumper} from '../common/tree_dumper.js';
import {Personality, QueueMode, TtsSettings, TtsSpeechProperties} from '../common/tts_types.js';

import {AutoScrollHandler} from './auto_scroll_handler.js';
import {BrailleBackground} from './braille/braille_background.js';
import {BrailleCaptionsBackground} from './braille/braille_captions_background.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxState} from './chromevox_state.js';
import {ChromeVoxBackground} from './classic_background.js';
import {ClipboardHandler} from './clipboard_handler.js';
import {Color} from './color.js';
import {CommandHandlerInterface} from './command_handler_interface.js';
import {DesktopAutomationInterface} from './desktop_automation_interface.js';
import {TypingEcho} from './editing/editable_text_base.js';
import {EventSourceState} from './event_source.js';
import {GestureInterface} from './gesture_interface.js';
import {LogStore} from './logging/log_store.js';
import {Output} from './output/output.js';
import {OutputCustomEvent} from './output/output_types.js';
import {PhoneticData} from './phonetic_data.js';
import {ChromeVoxPrefs} from './prefs.js';
import {SmartStickyMode} from './smart_sticky_mode.js';
import {TtsBackground} from './tts_background.js';

const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;

/**
 * @typedef {{
 *   node: (AutomationNode|undefined),
 *   range: !CursorRange}}
 */
let NewRangeData;

export class CommandHandler extends CommandHandlerInterface {
  /** @private */
  constructor() {
    super();

    /**
     * To support viewGraphicAsBraille_(), the current image node.
     * @type {?AutomationNode}
     */
    this.imageNode_;

    /** @private {boolean} */
    this.isIncognito_ = Boolean(chrome.runtime.getManifest()['incognito']);

    /** @private {boolean} */
    this.isKioskSession_ = false;

    /** @private {boolean} */
    this.languageLoggingEnabled_ = false;

    SmartStickyMode.init();
    this.init_();
  }

  /** @private */
  init_() {
    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-language-detection', enabled => {
          if (enabled) {
            this.languageLoggingEnabled_ = true;
          }
        });
    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-language-detection-dynamic',
        enabled => {
          if (enabled) {
            this.languageLoggingEnabled_ = true;
          }
        });

    chrome.chromeosInfoPrivate.get(['sessionType'], result => {
      /** @type {boolean} */
      this.isKioskSession_ = result['sessionType'] ===
          chrome.chromeosInfoPrivate.SessionType.KIOSK;
    });
  }

  /** @override */
  onCommand(command) {
    // Check for a command denied in incognito contexts and kiosk.
    if (!this.isAllowed_(command)) {
      return true;
    }

    // Check for loss of focus which results in us invalidating our current
    // range. Note this call is synchronous.
    chrome.automation.getFocus(focus => this.checkForLossOfFocus_(focus));

    // These commands don't require a current range.
    switch (command) {
      case Command.SPEAK_TIME_AND_DATE:
        this.speakTimeAndDate_();
        return false;
      case Command.SHOW_OPTIONS_PAGE:
        chrome.runtime.openOptionsPage();
        break;
      case Command.TOGGLE_STICKY_MODE:
        this.toggleStickyMode_();
        return false;
      case Command.PASS_THROUGH_MODE:
        ChromeVox.passThroughMode = true;
        ChromeVox.tts.speak(Msgs.getMsg('pass_through_key'), QueueMode.QUEUE);
        return true;
      case Command.SHOW_LEARN_MODE_PAGE:
        this.showLearnModePage_();
        break;
      case Command.SHOW_LOG_PAGE:
        const logPage = {url: 'chromevox/log_page/log.html'};
        chrome.tabs.create(logPage);
        break;
      case Command.ENABLE_LOGGING:
        this.enableLogging_();
        break;
      case Command.DISABLE_LOGGING:
        this.disableLogging_();
        break;
      case Command.DUMP_TREE:
        chrome.automation.getDesktop(
            root => LogStore.instance.writeTreeLog(new TreeDumper(root)));
        break;
      case Command.DECREASE_TTS_RATE:
        this.increaseOrDecreaseSpeechProperty_(TtsSettings.RATE, false);
        return false;
      case Command.INCREASE_TTS_RATE:
        this.increaseOrDecreaseSpeechProperty_(TtsSettings.RATE, true);
        return false;
      case Command.DECREASE_TTS_PITCH:
        this.increaseOrDecreaseSpeechProperty_(TtsSettings.PITCH, false);
        return false;
      case Command.INCREASE_TTS_PITCH:
        this.increaseOrDecreaseSpeechProperty_(TtsSettings.PITCH, true);
        return false;
      case Command.DECREASE_TTS_VOLUME:
        this.increaseOrDecreaseSpeechProperty_(TtsSettings.VOLUME, false);
        return false;
      case Command.INCREASE_TTS_VOLUME:
        this.increaseOrDecreaseSpeechProperty_(TtsSettings.VOLUME, true);
        return false;
      case Command.STOP_SPEECH:
        ChromeVox.tts.stop();
        ChromeVoxState.instance.isReadingContinuously = false;
        return false;
      case Command.TOGGLE_EARCONS:
        this.toggleEarcons_();
        return false;
      case Command.CYCLE_TYPING_ECHO:
        this.cycleTypingEcho_();
        return false;
      case Command.CYCLE_PUNCTUATION_ECHO:
        ChromeVox.tts.speak(
            Msgs.getMsg(TtsBackground.base.cyclePunctuationEcho()),
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
        this.toggleBrailleTable_();
        return false;
      case Command.HELP:
        (new PanelCommand(PanelCommandType.TUTORIAL)).send();
        return false;
      case Command.TOGGLE_SCREEN:
        this.toggleScreen_();
        return false;
      case Command.TOGGLE_SPEECH_ON_OR_OFF:
        const state = ChromeVox.tts.toggleSpeechOnOrOff();
        new Output().format(state ? '@speech_on' : '@speech_off').go();
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
        ChromeVox.tts.resetTextToSpeechSettings();
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
    }

    // The remaining commands require a current range.
    if (!ChromeVoxState.instance.currentRange) {
      if (!ChromeVoxState.instance.talkBackEnabled) {
        this.announceNoCurrentRange_();
      }
      return true;
    }

    // Allow edit commands first.
    if (!this.onEditCommand_(command)) {
      return false;
    }

    let currentRange = ChromeVoxState.instance.currentRange;
    let node = currentRange.start.node;

    // If true, will check if the predicate matches the current node.
    let matchCurrent = false;

    let dir = Dir.FORWARD;
    let newRangeData;
    let pred = null;
    let predErrorMsg = undefined;
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
        DesktopAutomationInterface.instance.onNativeNextOrPreviousCharacter();
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
        DesktopAutomationInterface.instance.onNativeNextOrPreviousWord(
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
        SmartStickyMode.instance.startIgnoringRangeChanges();
        break;
      case Command.PREVIOUS_EDIT_TEXT:
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.editText;
        predErrorMsg = 'no_previous_edit_text';
        SmartStickyMode.instance.startIgnoringRangeChanges();
        break;
      case Command.NEXT_FORM_FIELD:
        skipSettingSelection = true;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_next_form_field';
        SmartStickyMode.instance.startIgnoringRangeChanges();
        break;
      case Command.PREVIOUS_FORM_FIELD:
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_previous_form_field';
        SmartStickyMode.instance.startIgnoringRangeChanges();
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
      case Command.LEFT:
      case Command.PREVIOUS_OBJECT:
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        // Falls through.
      case Command.RIGHT:
      case Command.NEXT_OBJECT:
        skipSettingSelection = true;
        didNavigate = true;
        unit = (EventSourceState.get() === EventSourceType.TOUCH_GESTURE) ?
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
        // Falls through.
      case Command.NEXT_SIMILAR_ITEM:
        skipSync = true;
        pred = this.getPredicateForNextOrPreviousSimilarItem_(node);
        break;
      case Command.PREVIOUS_INVALID_ITEM:
        dir = Dir.BACKWARD;
        // Falls through.
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
        new Output().withString(node.root.docUrl || '').go();
        return false;
      case Command.TOGGLE_SELECTION:
        if (!this.toggleSelection_()) {
          return false;
        }
        break;
      case Command.FULLY_DESCRIBE:
        const o = new Output();
        o.withContextFirst()
            .withRichSpeechAndBraille(
                currentRange, null, OutputCustomEvent.NAVIGATE)
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
        this.readLinkURL_(node);
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

      let bound = currentRange.getBound(dir).node;
      if (bound) {
        let node = null;

        if (matchCurrent && pred(bound)) {
          node = bound;
        }

        if (!node) {
          node = AutomationUtil.findNextNode(
              bound, dir, pred, {skipInitialAncestry, root: rootPred});
        }

        if (node && !skipSync) {
          node = AutomationUtil.findNodePre(
                     node, Dir.FORWARD, AutomationPredicate.object) ||
              node;
        }

        if (node) {
          currentRange = CursorRange.fromNode(node);
        } else {
          ChromeVox.earcons.playEarcon(Earcon.WRAP);
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

          let root = bound;
          while (root && !AutomationPredicate.rootOrEditableRoot(root)) {
            root = root.parent;
          }

          if (!root) {
            root = bound.root;
          }

          if (dir === Dir.FORWARD) {
            bound = root;
          } else {
            bound = AutomationUtil.findNodePost(
                        /** @type {!AutomationNode} */ (root), dir,
                        AutomationPredicate.leaf) ||
                bound;
          }
          node = AutomationUtil.findNextNode(
              /** @type {!AutomationNode} */ (bound), dir, pred,
              {root: rootPred});

          if (node && !skipSync) {
            node = AutomationUtil.findNodePre(
                       node, Dir.FORWARD, AutomationPredicate.object) ||
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
    if (tryScrolling && currentRange &&
        !AutoScrollHandler.instance.onCommandNavigation(
            currentRange, dir, pred, unit, speechProps, rootPred, () => {
              this.onCommand(command);
              this.onFinishCommand();
            })) {
      this.onFinishCommand();
      return false;
    }

    if (currentRange) {
      if (currentRange.wrapped) {
        ChromeVox.earcons.playEarcon(Earcon.WRAP);
      }

      ChromeVoxState.instance.navigateToRange(
          currentRange, undefined, speechProps, skipSettingSelection);
    }

    this.onFinishCommand();
    return false;
  }

  /**
   * Finishes processing of a command.
   */
  onFinishCommand() {
    SmartStickyMode.instance.stopIgnoringRangeChanges();
  }

  /**
   * Increase or decrease a speech property and make an announcement.
   * @param {string} propertyName The name of the property to change.
   * @param {boolean} increase If true, increases the property value by one
   *     step size, otherwise decreases.
   * @private
   */
  increaseOrDecreaseSpeechProperty_(propertyName, increase) {
    ChromeVox.tts.increaseOrDecreaseProperty(propertyName, increase);
  }

  /**
   * Called when an image frame is received on a node.
   * @param {!ChromeVoxEvent} event The event.
   * @private
   */
  onImageFrameUpdated_(event) {
    const target = event.target;
    if (target !== this.imageNode_) {
      return;
    }

    if (!AutomationUtil.isDescendantOf(
            ChromeVoxState.instance.currentRange.start.node, this.imageNode_)) {
      this.imageNode_.removeEventListener(
          EventType.IMAGE_FRAME_UPDATED, this.onImageFrameUpdated_, false);
      this.imageNode_ = null;
      return;
    }

    if (target.imageDataUrl) {
      ChromeVox.braille.writeRawImage(target.imageDataUrl);
      ChromeVox.braille.freeze();
    }
  }

  /**
   * Handle the command to view the first graphic within the current range
   * as braille.
   * @param {!CursorRange} currentRange The current range.
   * @private
   */
  viewGraphicAsBraille_(currentRange) {
    if (this.imageNode_) {
      this.imageNode_.removeEventListener(
          EventType.IMAGE_FRAME_UPDATED, this.onImageFrameUpdated_, false);
      this.imageNode_ = null;
    }

    // Find the first node within the current range that supports image data.
    const imageNode = AutomationUtil.findNodePost(
        currentRange.start.node, Dir.FORWARD,
        AutomationPredicate.supportsImageData);
    if (!imageNode) {
      return;
    }

    imageNode.addEventListener(
        EventType.IMAGE_FRAME_UPDATED, this.onImageFrameUpdated_, false);
    this.imageNode_ = imageNode;
    if (imageNode.imageDataUrl) {
      const event = new CustomAutomationEvent(
          EventType.IMAGE_FRAME_UPDATED, imageNode, {eventFrom: 'page'});
      this.onImageFrameUpdated_(event);
    } else {
      imageNode.getImageData(0, 0);
    }
  }

  /**
   * Provides a partial mapping from ChromeVox key combinations to
   * Search-as-a-function key as seen in Chrome OS documentation.
   * @param {!Command} command
   * @return {boolean} True if the command should propagate.
   * @private
   */
  onEditCommand_(command) {
    if (ChromeVoxPrefs.isStickyModeOn()) {
      return true;
    }

    const textEditHandler = DesktopAutomationInterface.instance.textEditHandler;
    if (!textEditHandler ||
        !AutomationUtil.isDescendantOf(
            ChromeVoxState.instance.currentRange.start.node,
            textEditHandler.node)) {
      return true;
    }

    // Skip customized keys for read only text fields.
    if (textEditHandler.node.restriction ===
        chrome.automation.Restriction.READ_ONLY) {
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
          ChromeVoxState.instance.setCurrentRange(
              CursorRange.fromNode(textEditHandler.node));
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
          ChromeVoxState.instance.setCurrentRange(
              CursorRange.fromNode(textEditHandler.node));
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

  /** @override */
  skipLabelOrDescriptionFor(currentRange, dir) {
    if (!currentRange) {
      return null;
    }

    // Keep moving past all nodes acting as labels or descriptions.
    while (currentRange && currentRange.start && currentRange.start.node &&
           currentRange.start.node.role === RoleType.STATIC_TEXT) {
      // We must scan upwards as any ancestor might have a label or description.
      let ancestor = currentRange.start.node;
      while (ancestor) {
        if ((ancestor.labelFor && ancestor.labelFor.length > 0) ||
            (ancestor.descriptionFor && ancestor.descriptionFor.length > 0)) {
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

  /** @private */
  announceBatteryDescription_() {
    chrome.accessibilityPrivate.getBatteryDescription(batteryDescription => {
      new Output()
          .withString(batteryDescription)
          .withQueueMode(QueueMode.FLUSH)
          .go();
    });
  }

  /** @private */
  announceNoCurrentRange_() {
    new Output()
        .withString(Msgs.getMsg(
            EventSourceState.get() === EventSourceType.TOUCH_GESTURE ?
                'no_focus_touch' :
                'no_focus'))
        .withQueueMode(QueueMode.FLUSH)
        .go();
  }

  /**
   * @param {!AutomationNode} node
   * @private
   */
  announceRichTextDescription_(node) {
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

  /**
   * @param {AutomationNode} focusedNode
   * @private
   */
  checkForLossOfFocus_(focusedNode) {
    const cur = ChromeVoxState.instance.currentRange;
    if (cur && !cur.isValid() && focusedNode) {
      ChromeVoxState.instance.setCurrentRange(
          CursorRange.fromNode(focusedNode));
    }

    if (!focusedNode) {
      ChromeVoxState.instance.setCurrentRange(null);
      return;
    }

    // This case detects when TalkBack (in ARC++) is enabled (which also
    // covers when the ARC++ window is active). Clear the ChromeVox range
    // so keys get passed through for ChromeVox commands.
    if (ChromeVoxState.instance.talkBackEnabled &&
        // This additional check is not strictly necessary, but we use it to
        // ensure we are never inadvertently losing focus. ARC++ windows set
        // "focus" on a root view.
        focusedNode.role === RoleType.CLIENT) {
      ChromeVoxState.instance.setCurrentRange(null);
    }
  }

  /** @private */
  cycleTypingEcho_() {
    LocalStorage.set(
        'typingEcho', TypingEcho.cycle(LocalStorage.get('typingEcho')));
    let announce = '';
    switch (LocalStorage.get('typingEcho')) {
      case TypingEcho.CHARACTER:
        announce = Msgs.getMsg('character_echo');
        break;
      case TypingEcho.WORD:
        announce = Msgs.getMsg('word_echo');
        break;
      case TypingEcho.CHARACTER_AND_WORD:
        announce = Msgs.getMsg('character_and_word_echo');
        break;
      case TypingEcho.NONE:
        announce = Msgs.getMsg('none_echo');
        break;
    }
    ChromeVox.tts.speak(announce, QueueMode.FLUSH, Personality.ANNOTATION);
  }

  /** @private */
  disableChromeVoxArcSupportForCurrentApp_() {
    chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
        false, response => {
          if (response ===
              chrome.accessibilityPrivate.SetNativeChromeVoxResponse
                  .TALKBACK_NOT_INSTALLED) {
            ChromeVox.braille.write(
                NavBraille.fromText(Msgs.getMsg('announce_install_talkback')));
            ChromeVox.tts.speak(
                Msgs.getMsg('announce_install_talkback'), QueueMode.FLUSH);
          } else if (
              response ===
              chrome.accessibilityPrivate.SetNativeChromeVoxResponse
                  .NEED_DEPRECATION_CONFIRMATION) {
            ChromeVox.braille.write(NavBraille.fromText(
                Msgs.getMsg('announce_talkback_deprecation')));
            ChromeVox.tts.speak(
                Msgs.getMsg('announce_talkback_deprecation'), QueueMode.FLUSH);
          }
        });
  }

  /** @private */
  disableLogging_() {
    for (const type in ChromeVoxPrefs.loggingPrefs) {
      ChromeVoxPrefs.instance.setLoggingPrefs(
          ChromeVoxPrefs.loggingPrefs[type], false);
    }
  }

  /** @private */
  enableChromeVoxArcSupportForCurrentApp_() {
    chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
        true, response => {});
  }

  /** @private */
  enableLogging_() {
    for (const type in ChromeVoxPrefs.loggingPrefs) {
      ChromeVoxPrefs.instance.setLoggingPrefs(
          ChromeVoxPrefs.loggingPrefs[type], true);
    }
  }

  /** @private */
  forceClickOnCurrentItem_() {
    if (!ChromeVoxState.instance.currentRange) {
      return;
    }
    let actionNode = ChromeVoxState.instance.currentRange.start.node;
    // Scan for a clickable, which overrides the |actionNode|.
    let clickable = actionNode;
    while (clickable && !clickable.clickable &&
           actionNode.root === clickable.root) {
      clickable = clickable.parent;
    }
    if (clickable && actionNode.root === clickable.root) {
      clickable.doDefault();
      return;
    }

    if (EventSourceState.get() === EventSourceType.TOUCH_GESTURE &&
        actionNode.state.editable) {
      // Dispatch a click to ensure the VK gets shown.
      const center = RectUtil.center(actionNode.location);
      EventGenerator.sendMouseClick(center.x, center.y);
      return;
    }

    while (actionNode.role === RoleType.INLINE_TEXT_BOX ||
           actionNode.role === RoleType.STATIC_TEXT) {
      actionNode = actionNode.parent;
    }
    if (actionNode.inPageLinkTarget) {
      ChromeVoxState.instance.navigateToRange(
          CursorRange.fromNode(actionNode.inPageLinkTarget));
      return;
    }
    actionNode.doDefault();
  }

  /**
   * @param {!AutomationNode} node
   * @return {!NewRangeData}
   * @private
   */
  getNewRangeForGoToColLastCell_(node) {
    // Try to start on the last cell of the table and allow
    // matching that node.
    let startNode = node.lastChild;
    while (startNode.lastChild && !AutomationPredicate.cellLike(startNode)) {
      startNode = startNode.lastChild;
    }
    return {node: startNode, range: CursorRange.fromNode(startNode)};
  }

  /**
   * @param {!AutomationNode} node
   * @param {!CursorRange} currentRange
   * @param {!Command} command
   * @return {!NewRangeData}
   * @private
   */
  getNewRangeForGoToFirstOrLastCell_(node, currentRange, command) {
    const end = AutomationUtil.findNodePost(
        node, command === Command.GO_TO_LAST_CELL ? Dir.BACKWARD : Dir.FORWARD,
        AutomationPredicate.leaf);
    if (end) {
      return {node: end, range: CursorRange.fromNode(end)};
    }
    return {node, range: currentRange};
  }

  /**
   * @param {!CursorRange} currentRange
   * @return {!NewRangeData}
   * @private
   */
  getNewRangeForJumpToBottom_(node, currentRange) {
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

  /**
   * @param {!CursorRange} currentRange
   * @return {!NewRangeData}
   * @private
   */
  getNewRangeForJumpToTop_(node, currentRange) {
    if (!currentRange.start.node || !currentRange.start.node.root) {
      return {node, range: currentRange};
    }
    const newNode = AutomationUtil.findNodePost(
        currentRange.start.node.root, Dir.FORWARD, AutomationPredicate.object);
    if (newNode) {
      return {node: newNode, range: CursorRange.fromNode(newNode)};
    }
    return {node, range: currentRange};
  }

  /**
   * @param {!AutomationNode} node
   * @param {!CursorRange} currentRange
   * @param {!Command} command
   * @return {!NewRangeData}
   * @private
   */
  getNewRangeForGoToRowFirstOrLastCell_(node, currentRange, command) {
    let current = node;
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

  /**
   * @param {AutomationNode} node
   * @param {!CursorRange} currentRange
   * @return {!NewRangeData}
   * @private
   */
  getNewRangeForJumpToDetails_(node, currentRange) {
    let current = node;
    while (current && !current.details) {
      current = current.parent;
    }
    if (current && current.details.length) {
      // TODO currently can only jump to first detail.
      currentRange = CursorRange.fromNode(current.details[0]);
    }
    return {node: current, range: currentRange};
  }

  /**
   * @param {!CursorRange} currentRange
   * @param {constants.Dir} dir
   * @return {?AutomationPredicate.Unary}
   * @private
   */
  getPredicateForGoToColFirstOrLastCell_(currentRange, dir) {
    const tableOpts = {col: true, dir, end: true};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  /**
   * @param {!CursorRange} currentRange
   * @param {constants.Dir} dir
   * @return {?AutomationPredicate.Unary}
   * @private
   */
  getPredicateForNextCol_(currentRange, dir) {
    const tableOpts = {col: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  /**
   * @param {!CursorRange} currentRange
   * @param {constants.Dir} dir
   * @return {?AutomationPredicate.Unary}
   * @private
   */
  getPredicateForNextRow_(currentRange, dir) {
    const tableOpts = {row: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  /**
   * @param {!AutomationNode} node
   * @return {?AutomationPredicate.Unary}
   * @private
   */
  getPredicateForNextOrPreviousSimilarItem_(node) {
    const originalNode = node;
    let current = node;

    // Scan upwards until we get a role we don't want to ignore.
    while (current && AutomationPredicate.ignoreDuringJump(current)) {
      current = current.parent;
    }

    const useNode = current || originalNode;
    return AutomationPredicate.roles([current.role]);
  }

  /**
   * @param {!CursorRange} currentRange
   * @param {!constants.Dir} dir
   * @return {?AutomationPredicate.Unary}
   * @private
   */
  getPredicateForPreviousCol_(currentRange, dir) {
    const tableOpts = {col: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  /**
   * @param {!CursorRange} currentRange
   * @param {!constants.Dir} dir
   * @return {?AutomationPredicate.Unary}
   * @private
   */
  getPredicateForPreviousRow_(currentRange, dir) {
    const tableOpts = {row: true, dir};
    return AutomationPredicate.makeTableCellPredicate(
        currentRange.start.node, tableOpts);
  }

  /**
   * @param {AutomationNode} node
   * @return {AutomationNode|undefined} node
   * @private
   */
  getTableNode_(node) {
    let current = node;
    while (current && current.role !== RoleType.TABLE) {
      current = current.parent;
    }
    return current;
  }

  /**
   * @param {!Command} command
   * @return {boolean}
   * @private
   */
  isAllowed_(command) {
    if (!this.isIncognito_ && !this.isKioskSession_) {
      return true;
    }

    return !CommandStore.CMD_ALLOWLIST[command] ||
        !CommandStore.CMD_ALLOWLIST[command].denySignedOut;
  }

  /**
   * @param {boolean} isPrevious
   * @private
   */
  nextOrPreviousAtGranularity_(isPrevious) {
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

  /**
   * @param {!Command} command
   * @param {!CursorRange} currentRange
   * @private
   */
  nextOrPreviousPage_(command, currentRange) {
    const root = AutomationUtil.getTopLevelRoot(currentRange.start.node);
    if (root && root.scrollY !== undefined) {
      let page = Math.ceil(root.scrollY / root.location.height) || 1;
      page = command === Command.NEXT_PAGE ? page + 1 : page - 1;
      ChromeVox.tts.stop();
      root.setScrollOffset(0, page * root.location.height);
    }
  }

  /** @private */
  readCurrentTitle_() {
    let target = ChromeVoxState.instance.currentRange.start.node;
    const output = new Output();

    if (!target) {
      return false;
    }

    let firstWindow;
    let rootViewWindow;
    if (target.root && target.root.role === RoleType.DESKTOP) {
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
    target = rootViewWindow || firstWindow || target;

    if (!target) {
      output.format('@no_title');
    } else if (target.name) {
      output.withString(target.name);
    }

    output.go();
  }

  /** @private */
  readFromHere_() {
    ChromeVoxState.instance.isReadingContinuously = true;
    const continueReading = () => {
      if (!ChromeVoxState.instance.isReadingContinuously ||
          !ChromeVoxState.instance.currentRange) {
        return;
      }

      const prevRange = ChromeVoxState.instance.currentRange;
      const newRange = ChromeVoxState.instance.currentRange.move(
          CursorUnit.NODE, Dir.FORWARD);

      // Stop if we've wrapped back to the document.
      const maybeDoc = newRange.start.node;
      if (AutomationPredicate.root(maybeDoc)) {
        ChromeVoxState.instance.isReadingContinuously = false;
        return;
      }

      ChromeVoxState.instance.setCurrentRange(newRange);
      newRange.select();

      const o = new Output()
                    .withoutHints()
                    .withRichSpeechAndBraille(
                        ChromeVoxState.instance.currentRange, prevRange,
                        OutputCustomEvent.NAVIGATE)
                    .onSpeechEnd(continueReading);

      if (!o.hasSpeech) {
        continueReading();
        return;
      }

      o.go();
    };

    {
      const startNode = ChromeVoxState.instance.currentRange.start.node;
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

  /**
   * @param {!AutomationNode} node
   * @private
   */
  readLinkURL_(node) {
    const rootNode = node.root;
    let current = node;
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

  /**
   * @param {!AutomationNode} node
   * @private
   */
  readPhoneticPronunciation_(node) {
    // Get node info.
    const index = ChromeVoxState.instance.currentRange.start.index;
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
    let wordStarts;
    let wordEnds;
    if (node.role === RoleType.INLINE_TEXT_BOX) {
      wordStarts = node.wordStarts;
      wordEnds = node.wordEnds;
    } else {
      wordStarts = node.nonInlineTextWordStarts;
      wordEnds = node.nonInlineTextWordEnds;
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

  /** @private */
  reportIssue_() {
    let url = 'https://code.google.com/p/chromium/issues/entry?' +
        'labels=Type-Bug,Pri-2,OS-Chrome&' +
        'components=OS>Accessibility>ChromeVox&' +
        'description=';

    const description = {};
    description['Version'] = chrome.runtime.getManifest().version;
    description['Reproduction Steps'] = '%0a1.%0a2.%0a3.';
    for (const key in description) {
      url += key + ':%20' + description[key] + '%0a';
    }
    chrome.tabs.create({url});
  }

  /** @private */
  showLearnModePage_() {
    const explorerPage = {
      url: 'chromevox/learn_mode/learn_mode.html',
      type: 'panel',
    };
    chrome.windows.create(explorerPage);
  }

  /** @private */
  showTalkBackKeyboardShortcuts_() {
    chrome.tabs.create({
      url: 'https://support.google.com/accessibility/android/answer/6110948',
    });
  }

  /** @private */
  speakTimeAndDate_() {
    chrome.automation.getDesktop(d => {
      // First, try speaking the on-screen time.
      const allTime = d.findAll({role: RoleType.TIME});
      allTime.filter(time => time.root.role === RoleType.DESKTOP);

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
   * @param {boolean} isPrevious
   * @private
   */
  nextOrPreviousGranularity_(isPrevious) {
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

  /** @private */
  toggleBrailleTable_() {
    let brailleTableType = LocalStorage.get('brailleTableType');
    let output = '';
    if (brailleTableType === 'brailleTable6') {
      brailleTableType = 'brailleTable8';

      // This label reads "switch to 8 dot braille".
      output = '@OPTIONS_BRAILLE_TABLE_TYPE_6';
    } else {
      brailleTableType = 'brailleTable6';

      // This label reads "switch to 6 dot braille".
      output = '@OPTIONS_BRAILLE_TABLE_TYPE_8';
    }

    LocalStorage.set('brailleTable', LocalStorage.get(brailleTableType));
    LocalStorage.set('brailleTableType', brailleTableType);
    BrailleBackground.instance.getTranslatorManager().refresh(
        LocalStorage.get(brailleTableType));
    new Output().format(output).go();
  }

  /** @private */
  toggleEarcons_() {
    ChromeVox.earcons.enabled = !ChromeVox.earcons.enabled;
    const announce = ChromeVox.earcons.enabled ? Msgs.getMsg('earcons_on') :
                                                 Msgs.getMsg('earcons_off');
    ChromeVox.tts.speak(announce, QueueMode.FLUSH, Personality.ANNOTATION);
  }

  /** @private */
  toggleScreen_() {
    const newState = !ChromeVoxPrefs.darkScreen;
    if (newState && !LocalStorage.get('acceptToggleScreen')) {
      // If this is the first time, show a confirmation dialog.
      chrome.accessibilityPrivate.showConfirmationDialog(
          Msgs.getMsg('toggle_screen_title'),
          Msgs.getMsg('toggle_screen_description'), confirmed => {
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

  /**
   * @return {boolean} whether execution should continue.
   * @private
   */
  toggleSelection_() {
    if (!ChromeVoxState.instance.pageSel) {
      ChromeVoxState.instance.pageSel = ChromeVoxState.instance.currentRange;
      DesktopAutomationInterface.instance.ignoreDocumentSelectionFromAction(
          true);
    } else {
      const root = ChromeVoxState.instance.currentRange.start.node.root;
      if (root && root.selectionStartObject && root.selectionEndObject &&
          !isNaN(Number(root.selectionStartOffset)) &&
          !isNaN(Number(root.selectionEndOffset))) {
        const sel = new CursorRange(
            new Cursor(
                root.selectionStartObject,
                /** @type {number} */ (root.selectionStartOffset)),
            new Cursor(
                root.selectionEndObject,
                /** @type {number} */ (root.selectionEndOffset)));
        const o =
            new Output()
                .format('@end_selection')
                .withSpeechAndBraille(sel, sel, OutputCustomEvent.NAVIGATE)
                .go();
        DesktopAutomationInterface.instance.ignoreDocumentSelectionFromAction(
            false);
      }
      ChromeVoxState.instance.pageSel = null;
      return false;
    }
    return true;
  }

  /** @private */
  toggleStickyMode_() {
    ChromeVoxPrefs.instance.setAndAnnounceStickyPref(
        !ChromeVoxPrefs.isStickyPrefOn);

    if (ChromeVoxState.instance.currentRange) {
      SmartStickyMode.instance.onStickyModeCommand(
          ChromeVoxState.instance.currentRange);
    }
  }

  /**
   * Performs global initialization.
   */
  static init() {
    CommandHandlerInterface.instance = new CommandHandler();
    ChromeVoxKbHandler.commandHandler = command =>
        CommandHandlerInterface.instance.onCommand(command);

    BridgeHelper.registerHandler(
        BridgeConstants.CommandHandler.TARGET,
        BridgeConstants.CommandHandler.Action.ON_COMMAND, command => {
          if (Object.values(Command).includes(command)) {
            CommandHandlerInterface.instance.onCommand(
                /** @type {Command} */ (command));
          } else {
            console.warn('ChromeVox got an unrecognized command: ' + command);
          }
        });
  }
}
