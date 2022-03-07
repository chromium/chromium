// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox commands.
 */
import {ChromeVoxBackground} from './classic_background.js';
import {Color} from './color.js';
import {DesktopAutomationInterface} from './desktop_automation_interface.js';
import {GestureInterface} from './gesture_interface.js';
import {SmartStickyMode} from './smart_sticky_mode.js';

const ActionType = chrome.automation.ActionType;
const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

export class CommandHandler extends CommandHandlerInterface {
  /** @private */
  constructor() {
    super();

    /** @private {boolean} */
    this.isIncognito_ = !!chrome.runtime.getManifest()['incognito'];

    /** @private {boolean} */
    this.languageLoggingEnabled_ = false;

    /**
     * Handles toggling sticky mode when encountering editables.
     * @private {!SmartStickyMode}
     */
    this.smartStickyMode_ = new SmartStickyMode();

    /**
     * To support viewGraphicAsBraille_(), the current image node.
     * @type {AutomationNode?};
     */
    this.imageNode_;

    /** @private {boolean} */
    this.isKioskSession_ = false;

    this.init();
  }

  /** @override */
  onCommand(command) {
    // Check for a command denied in incognito contexts and kiosk.
    if ((this.isIncognito_ || this.isKioskSession_) &&
        CommandStore.CMD_ALLOWLIST[command] &&
        CommandStore.CMD_ALLOWLIST[command].denyOOBE) {
      return true;
    }

    // Check for loss of focus which results in us invalidating our current
    // range. Note this call is synchronous.
    chrome.automation.getFocus(function(focusedNode) {
      const cur = ChromeVoxState.instance.currentRange;
      if (cur && !cur.isValid()) {
        ChromeVoxState.instance.setCurrentRange(
            cursors.Range.fromNode(focusedNode));
      }

      if (!focusedNode ||

          // This case detects when TalkBack (in ARC++) is enabled (which also
          // covers when the ARC++ window is active). Clear the ChromeVox range
          // so keys get passed through for ChromeVox commands.
          (ChromeVoxState.instance.talkBackEnabled &&

           // This additional check is not strictly necessary, but we use it to
           // ensure we are never inadvertently losing focus. ARC++ windows set
           // "focus" on a root view.
           focusedNode.role === RoleType.CLIENT)) {
        ChromeVoxState.instance.setCurrentRange(null);
      }
    });

    // These commands don't require a current range.
    switch (command) {
      case 'speakTimeAndDate':
        chrome.automation.getDesktop(function(d) {
          // First, try speaking the on-screen time.
          const allTime = d.findAll({role: RoleType.TIME});
          allTime.filter(function(t) {
            return t.root.role === RoleType.DESKTOP;
          });

          let timeString = '';
          allTime.forEach(function(t) {
            if (t.name) {
              timeString = t.name;
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
        return false;
      case 'showOptionsPage':
        chrome.runtime.openOptionsPage();
        break;
      case 'toggleStickyMode':
        ChromeVoxBackground.setPref('sticky', !ChromeVox.isStickyPrefOn, true);
        this.smartStickyMode_.onStickyModeCommand(
            ChromeVoxState.instance.currentRange);
        return false;
      case 'passThroughMode':
        ChromeVox.passThroughMode = true;
        ChromeVox.tts.speak(Msgs.getMsg('pass_through_key'), QueueMode.QUEUE);
        return true;
      case 'showKbExplorerPage':
        const explorerPage = {
          url: 'chromevox/learn_mode/kbexplorer.html',
          type: 'panel'
        };
        chrome.windows.create(explorerPage);
        break;
      case 'showLogPage':
        const logPage = {url: 'chromevox/background/logging/log.html'};
        chrome.tabs.create(logPage);
        break;
      case 'enableLogging': {
        for (const type in ChromeVoxPrefs.loggingPrefs) {
          ChromeVoxPrefs.instance.setLoggingPrefs(
              ChromeVoxPrefs.loggingPrefs[type], true);
        }
      } break;
      case 'disableLogging': {
        for (const type in ChromeVoxPrefs.loggingPrefs) {
          ChromeVoxPrefs.instance.setLoggingPrefs(
              ChromeVoxPrefs.loggingPrefs[type], false);
        }
      } break;
      case 'dumpTree':
        chrome.automation.getDesktop(function(root) {
          LogStore.getInstance().writeTreeLog(new TreeDumper(root));
        });
        break;
      case 'decreaseTtsRate':
        this.increaseOrDecreaseSpeechProperty_(AbstractTts.RATE, false);
        return false;
      case 'increaseTtsRate':
        this.increaseOrDecreaseSpeechProperty_(AbstractTts.RATE, true);
        return false;
      case 'decreaseTtsPitch':
        this.increaseOrDecreaseSpeechProperty_(AbstractTts.PITCH, false);
        return false;
      case 'increaseTtsPitch':
        this.increaseOrDecreaseSpeechProperty_(AbstractTts.PITCH, true);
        return false;
      case 'decreaseTtsVolume':
        this.increaseOrDecreaseSpeechProperty_(AbstractTts.VOLUME, false);
        return false;
      case 'increaseTtsVolume':
        this.increaseOrDecreaseSpeechProperty_(AbstractTts.VOLUME, true);
        return false;
      case 'stopSpeech':
        ChromeVox.tts.stop();
        ChromeVoxState.isReadingContinuously = false;
        return false;
      case 'toggleEarcons': {
        ChromeVox.earcons.enabled = !ChromeVox.earcons.enabled;
        const announce = ChromeVox.earcons.enabled ? Msgs.getMsg('earcons_on') :
                                                     Msgs.getMsg('earcons_off');
        ChromeVox.tts.speak(
            announce, QueueMode.FLUSH, AbstractTts.PERSONALITY_ANNOTATION);
      }
        return false;
      case 'cycleTypingEcho': {
        ChromeVox.typingEcho = TypingEcho.cycle(ChromeVox.typingEcho);
        let announce = '';
        switch (ChromeVox.typingEcho) {
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
        ChromeVox.tts.speak(
            announce, QueueMode.FLUSH, AbstractTts.PERSONALITY_ANNOTATION);
      }
        return false;
      case 'cyclePunctuationEcho':
        ChromeVox.tts.speak(
            Msgs.getMsg(ChromeVoxState.backgroundTts.cyclePunctuationEcho()),
            QueueMode.FLUSH);
        return false;
      case 'reportIssue':
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
        return false;
      case 'toggleBrailleCaptions':
        BrailleCaptionsBackground.setActive(
            !BrailleCaptionsBackground.isEnabled());
        return false;
      case 'toggleBrailleTable': {
        let brailleTableType = localStorage['brailleTableType'];
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

        localStorage['brailleTable'] = localStorage[brailleTableType];
        localStorage['brailleTableType'] = brailleTableType;
        BrailleBackground.getInstance().getTranslatorManager().refresh(
            localStorage[brailleTableType]);
        new Output().format(output).go();
      }
        return false;
      case 'help':
        (new PanelCommand(PanelCommandType.TUTORIAL)).send();
        return false;
      case 'toggleScreen':
        const oldState = sessionStorage.getItem('darkScreen');
        const newState = (oldState === 'true') ? false : true;
        if (newState && localStorage['acceptToggleScreen'] !== 'true') {
          // If this is the first time, show a confirmation dialog.
          chrome.accessibilityPrivate.showConfirmationDialog(
              Msgs.getMsg('toggle_screen_title'),
              Msgs.getMsg('toggle_screen_description'), (confirmed) => {
                if (confirmed) {
                  sessionStorage.setItem('darkScreen', 'true');
                  localStorage['acceptToggleScreen'] = true;
                  chrome.accessibilityPrivate.darkenScreen(true);
                  new Output().format('@toggle_screen_off').go();
                }
              });
        } else {
          sessionStorage.setItem('darkScreen', (newState) ? 'true' : 'false');
          chrome.accessibilityPrivate.darkenScreen(newState);
          new Output()
              .format((newState) ? '@toggle_screen_off' : '@toggle_screen_on')
              .go();
        }
        return false;
      case 'toggleSpeechOnOrOff':
        const state = ChromeVox.tts.toggleSpeechOnOrOff();
        new Output().format(state ? '@speech_on' : '@speech_off').go();
        return false;
      case 'enableChromeVoxArcSupportForCurrentApp':
        chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
            true, (response) => {});
        break;
      case 'disableChromeVoxArcSupportForCurrentApp':
        chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
            false, (response) => {
              if (response ===
                  chrome.accessibilityPrivate.SetNativeChromeVoxResponse
                      .TALKBACK_NOT_INSTALLED) {
                ChromeVox.braille.write(NavBraille.fromText(
                    Msgs.getMsg('announce_install_talkback')));
                ChromeVox.tts.speak(
                    Msgs.getMsg('announce_install_talkback'), QueueMode.FLUSH);
              }
            });
        break;
      case 'showTtsSettings':
        chrome.accessibilityPrivate.openSettingsSubpage(
            'manageAccessibility/tts');
        break;
      default:
        break;
      case 'toggleKeyboardHelp':
        (new PanelCommand(PanelCommandType.OPEN_MENUS)).send();
        return false;
      case 'showPanelMenuMostRecent':
        (new PanelCommand(PanelCommandType.OPEN_MENUS_MOST_RECENT)).send();
        return false;
      case 'nextGranularity':
      case 'previousGranularity': {
        const backwards = command === 'previousGranularity';
        let gran = GestureInterface.getGranularity();
        const next = backwards ?
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
        return false;
      case 'announceBatteryDescription':
        chrome.accessibilityPrivate.getBatteryDescription(function(
            batteryDescription) {
          new Output()
              .withString(batteryDescription)
              .withQueueMode(QueueMode.FLUSH)
              .go();
        });
        break;
      case 'resetTextToSpeechSettings':
        ChromeVox.tts.resetTextToSpeechSettings();
        return false;
      case 'copy':
        EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});

        // The above command doesn't trigger document clipboard events, so we
        // need to set this manually.
        ChromeVoxState.instance.readNextClipboardDataChange();
        return false;
      case 'toggleDictation':
        EventGenerator.sendKeyPress(KeyCode.D, {search: true});
        return false;
    }

    // Require a current range.
    if (!ChromeVoxState.instance.currentRange_) {
      if (!ChromeVoxState.instance.talkBackEnabled) {
        new Output()
            .withString(Msgs.getMsg(
                EventSourceState.get() === EventSourceType.TOUCH_GESTURE ?
                    'no_focus_touch' :
                    'no_focus'))
            .withQueueMode(QueueMode.FLUSH)
            .go();
      }
      return true;
    }

    // Allow edit commands first.
    if (!this.onEditCommand_(command)) {
      return false;
    }

    let current = ChromeVoxState.instance.currentRange;
    let node = current.start.node;

    // If true, will check if the predicate matches the current node.
    let matchCurrent = false;

    let dir = Dir.FORWARD;
    let pred = null;
    let predErrorMsg = undefined;
    let rootPred = AutomationPredicate.rootOrEditableRoot;
    let unit = null;
    let shouldWrap = true;
    const speechProps = {};
    let skipSync = false;
    let didNavigate = false;
    let tryScrolling = true;
    let skipSettingSelection = false;
    let skipInitialAncestry = true;
    switch (command) {
      case 'nextCharacter':
        didNavigate = true;
        speechProps['phoneticCharacters'] = true;
        unit = cursors.Unit.CHARACTER;
        current = current.move(cursors.Unit.CHARACTER, Dir.FORWARD);
        break;
      case 'previousCharacter':
        dir = Dir.BACKWARD;
        didNavigate = true;
        speechProps['phoneticCharacters'] = true;
        unit = cursors.Unit.CHARACTER;
        current = current.move(cursors.Unit.CHARACTER, dir);
        break;
      case 'nativeNextCharacter':
      case 'nativePreviousCharacter':
        if (DesktopAutomationInterface.instance.textEditHandler) {
          DesktopAutomationInterface.instance.textEditHandler
              .injectInferredIntents([{
                command: chrome.automation.IntentCommandType.MOVE_SELECTION,
                textBoundary: chrome.automation.IntentTextBoundaryType.CHARACTER
              }]);
        }
        return true;
      case 'nextWord':
        didNavigate = true;
        unit = cursors.Unit.WORD;
        current = current.move(cursors.Unit.WORD, Dir.FORWARD);
        break;
      case 'previousWord':
        dir = Dir.BACKWARD;
        didNavigate = true;
        unit = cursors.Unit.WORD;
        current = current.move(cursors.Unit.WORD, dir);
        break;
      case 'nativeNextWord':
      case 'nativePreviousWord':
        if (DesktopAutomationInterface.instance.textEditHandler) {
          DesktopAutomationInterface.instance.textEditHandler
              .injectInferredIntents([{
                command: chrome.automation.IntentCommandType.MOVE_SELECTION,
                textBoundary: command === 'nativeNextWord' ?
                    chrome.automation.IntentTextBoundaryType.WORD_END :
                    chrome.automation.IntentTextBoundaryType.WORD_START
              }]);
        }
        return true;
      case 'forward':
      case 'nextLine':
        didNavigate = true;
        unit = cursors.Unit.LINE;
        current = current.move(cursors.Unit.LINE, Dir.FORWARD);
        break;
      case 'backward':
      case 'previousLine':
        dir = Dir.BACKWARD;
        didNavigate = true;
        unit = cursors.Unit.LINE;
        current = current.move(cursors.Unit.LINE, dir);
        break;
      case 'nextButton':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.button;
        predErrorMsg = 'no_next_button';
        break;
      case 'previousButton':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.button;
        predErrorMsg = 'no_previous_button';
        break;
      case 'nextCheckbox':
        pred = AutomationPredicate.checkBox;
        predErrorMsg = 'no_next_checkbox';
        break;
      case 'previousCheckbox':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.checkBox;
        predErrorMsg = 'no_previous_checkbox';
        break;
      case 'nextComboBox':
        pred = AutomationPredicate.comboBox;
        predErrorMsg = 'no_next_combo_box';
        break;
      case 'previousComboBox':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.comboBox;
        predErrorMsg = 'no_previous_combo_box';
        break;
      case 'nextEditText':
        skipSettingSelection = true;
        pred = AutomationPredicate.editText;
        predErrorMsg = 'no_next_edit_text';
        this.smartStickyMode_.startIgnoringRangeChanges();
        break;
      case 'previousEditText':
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.editText;
        predErrorMsg = 'no_previous_edit_text';
        this.smartStickyMode_.startIgnoringRangeChanges();
        break;
      case 'nextFormField':
        skipSettingSelection = true;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_next_form_field';
        this.smartStickyMode_.startIgnoringRangeChanges();
        break;
      case 'previousFormField':
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_previous_form_field';
        this.smartStickyMode_.startIgnoringRangeChanges();
        break;
      case 'previousGraphic':
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.image;
        predErrorMsg = 'no_previous_graphic';
        break;
      case 'nextGraphic':
        skipSettingSelection = true;
        pred = AutomationPredicate.image;
        predErrorMsg = 'no_next_graphic';
        break;
      case 'nextHeading':
        pred = AutomationPredicate.heading;
        predErrorMsg = 'no_next_heading';
        break;
      case 'nextHeading1':
        pred = AutomationPredicate.makeHeadingPredicate(1);
        predErrorMsg = 'no_next_heading_1';
        break;
      case 'nextHeading2':
        pred = AutomationPredicate.makeHeadingPredicate(2);
        predErrorMsg = 'no_next_heading_2';
        break;
      case 'nextHeading3':
        pred = AutomationPredicate.makeHeadingPredicate(3);
        predErrorMsg = 'no_next_heading_3';
        break;
      case 'nextHeading4':
        pred = AutomationPredicate.makeHeadingPredicate(4);
        predErrorMsg = 'no_next_heading_4';
        break;
      case 'nextHeading5':
        pred = AutomationPredicate.makeHeadingPredicate(5);
        predErrorMsg = 'no_next_heading_5';
        break;
      case 'nextHeading6':
        pred = AutomationPredicate.makeHeadingPredicate(6);
        predErrorMsg = 'no_next_heading_6';
        break;
      case 'previousHeading':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.heading;
        predErrorMsg = 'no_previous_heading';
        break;
      case 'previousHeading1':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(1);
        predErrorMsg = 'no_previous_heading_1';
        break;
      case 'previousHeading2':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(2);
        predErrorMsg = 'no_previous_heading_2';
        break;
      case 'previousHeading3':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(3);
        predErrorMsg = 'no_previous_heading_3';
        break;
      case 'previousHeading4':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(4);
        predErrorMsg = 'no_previous_heading_4';
        break;
      case 'previousHeading5':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(5);
        predErrorMsg = 'no_previous_heading_5';
        break;
      case 'previousHeading6':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeHeadingPredicate(6);
        predErrorMsg = 'no_previous_heading_6';
        break;
      case 'nextLink':
        pred = AutomationPredicate.link;
        predErrorMsg = 'no_next_link';
        break;
      case 'previousLink':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.link;
        predErrorMsg = 'no_previous_link';
        break;
      case 'nextTable':
        pred = AutomationPredicate.table;
        predErrorMsg = 'no_next_table';
        break;
      case 'previousTable':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.table;
        predErrorMsg = 'no_previous_table';
        break;
      case 'nextVisitedLink':
        pred = AutomationPredicate.visitedLink;
        predErrorMsg = 'no_next_visited_link';
        break;
      case 'previousVisitedLink':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.visitedLink;
        predErrorMsg = 'no_previous_visited_link';
        break;
      case 'nextLandmark':
        pred = AutomationPredicate.landmark;
        predErrorMsg = 'no_next_landmark';
        break;
      case 'previousLandmark':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.landmark;
        predErrorMsg = 'no_previous_landmark';
        break;
      case 'left':
      case 'previousObject':
        skipSettingSelection = true;
        dir = Dir.BACKWARD;
        // Falls through.
      case 'right':
      case 'nextObject':
        skipSettingSelection = true;
        didNavigate = true;
        unit = cursors.Unit.NODE;
        current = current.move(cursors.Unit.NODE, dir);
        current = this.skipLabelOrDescriptionFor(current, dir);
        break;
      case 'previousGroup':
        skipSync = true;
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.group;
        break;
      case 'nextGroup':
        skipSync = true;
        pred = AutomationPredicate.group;
        break;
      case 'previousPage':
      case 'nextPage':
        const root = AutomationUtil.getTopLevelRoot(current.start.node);
        if (root && root.scrollY !== undefined) {
          let page = Math.ceil(root.scrollY / root.location.height) || 1;
          page = command === 'nextPage' ? page + 1 : page - 1;
          ChromeVox.tts.stop();
          root.setScrollOffset(0, page * root.location.height);
        }
        return false;
      case 'previousSimilarItem':
        dir = Dir.BACKWARD;
        // Falls through.
      case 'nextSimilarItem': {
        skipSync = true;
        const originalNode = node;

        // Scan upwards until we get a role we don't want to ignore.
        while (node && AutomationPredicate.ignoreDuringJump(node)) {
          node = node.parent;
        }

        const useNode = node || originalNode;
        pred = AutomationPredicate.roles([node.role]);
      } break;
      case 'previousInvalidItem': {
        dir = Dir.BACKWARD;
        rootPred = AutomationPredicate.root;
        pred = AutomationPredicate.isInvalid;
        predErrorMsg = 'no_invalid_item';
      } break;
      case 'nextInvalidItem': {
        pred = AutomationPredicate.isInvalid;
        rootPred = AutomationPredicate.root;
        predErrorMsg = 'no_invalid_item';
      } break;
      case 'nextList':
        pred = AutomationPredicate.makeListPredicate(current.start.node);
        predErrorMsg = 'no_next_list';
        break;
      case 'previousList':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.makeListPredicate(current.start.node);
        predErrorMsg = 'no_previous_list';
        skipInitialAncestry = false;
        break;
      case 'jumpToTop': {
        const node = AutomationUtil.findNodePost(
            current.start.node.root, Dir.FORWARD, AutomationPredicate.object);
        if (node) {
          current = cursors.Range.fromNode(node);
        }
        tryScrolling = false;
      } break;
      case 'jumpToBottom': {
        const node = AutomationUtil.findLastNode(
            current.start.node.root, AutomationPredicate.object);
        if (node) {
          current = cursors.Range.fromNode(node);
        }
        tryScrolling = false;
      } break;
      case 'forceClickOnCurrentItem':
        if (ChromeVoxState.instance.currentRange) {
          let actionNode = ChromeVoxState.instance.currentRange.start.node;
          // Scan for a clickable, which overrides the |actionNode|.
          let clickable = actionNode;
          while (clickable && !clickable.clickable &&
                 actionNode.root === clickable.root) {
            clickable = clickable.parent;
          }
          if (clickable && actionNode.root === clickable.root) {
            clickable.doDefault();
            return false;
          }

          if (EventSourceState.get() === EventSourceType.TOUCH_GESTURE &&
              actionNode.state.editable) {
            // Dispatch a click to ensure the VK gets shown.
            const location = actionNode.location;
            EventGenerator.sendMouseClick(
                location.left + Math.round(location.width / 2),
                location.top + Math.round(location.height / 2));
            return false;
          }

          while (actionNode.role === RoleType.INLINE_TEXT_BOX ||
                 actionNode.role === RoleType.STATIC_TEXT) {
            actionNode = actionNode.parent;
          }
          if (actionNode.inPageLinkTarget) {
            ChromeVoxState.instance.navigateToRange(
                cursors.Range.fromNode(actionNode.inPageLinkTarget));
          } else {
            actionNode.doDefault();
          }
        }
        // Skip all other processing; if focus changes, we should get an event
        // for that.
        return false;
      case 'jumpToDetails': {
        while (node && !node.details) {
          node = node.parent;
        }
        if (node && node.details.length) {
          // TODO currently can only jump to first detail.
          current = cursors.Range.fromNode(node.details[0]);
        }
      } break;
      case 'readFromHere':
        ChromeVoxState.isReadingContinuously = true;
        const continueReading = function() {
          if (!ChromeVoxState.isReadingContinuously ||
              !ChromeVoxState.instance.currentRange) {
            return;
          }

          const prevRange = ChromeVoxState.instance.currentRange;
          const newRange = ChromeVoxState.instance.currentRange.move(
              cursors.Unit.NODE, Dir.FORWARD);

          // Stop if we've wrapped back to the document.
          const maybeDoc = newRange.start.node;
          if (AutomationPredicate.root(maybeDoc)) {
            ChromeVoxState.isReadingContinuously = false;
            return;
          }

          ChromeVoxState.instance.setCurrentRange(newRange);
          newRange.select();

          const o = new Output()
                        .withoutHints()
                        .withRichSpeechAndBraille(
                            ChromeVoxState.instance.currentRange, prevRange,
                            OutputEventType.NAVIGATE)
                        .onSpeechEnd(continueReading);

          if (!o.hasSpeech) {
            continueReading();
            return;
          }

          o.go();
        }.bind(this);

        {
          const startNode = ChromeVoxState.instance.currentRange.start.node;
          const collapsedRange = cursors.Range.fromNode(startNode);
          const o =
              new Output()
                  .withoutHints()
                  .withRichSpeechAndBraille(
                      collapsedRange, collapsedRange, OutputEventType.NAVIGATE)
                  .onSpeechEnd(continueReading);

          if (o.hasSpeech) {
            o.go();
          } else {
            continueReading();
          }
        }
        return false;
      case 'contextMenu':
        EventGenerator.sendKeyPress(KeyCode.APPS);
        break;
      case 'showHeadingsList':
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_heading')).send();
        return false;
      case 'showFormsList':
        (new PanelCommand(
             PanelCommandType.OPEN_MENUS, 'panel_menu_form_controls'))
            .send();
        return false;
      case 'showLandmarksList':
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_landmark')).send();
        return false;
      case 'showLinksList':
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_link')).send();
        return false;
      case 'showActionsMenu':
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'panel_menu_actions'))
            .send();
        return false;
      case 'showTablesList':
        (new PanelCommand(PanelCommandType.OPEN_MENUS, 'table_strategy'))
            .send();
        return false;
      case 'toggleSearchWidget':
        (new PanelCommand(PanelCommandType.SEARCH)).send();
        return false;
      case 'readCurrentTitle': {
        let target = ChromeVoxState.instance.currentRange.start.node;
        const output = new Output();

        if (!target) {
          return false;
        }

        let firstWindow;
        let rootViewWindow;
        if (target.root && target.root.role === RoleType.DESKTOP) {
          // Search for the first container with a name.
          while (target &&
                 (!target.name || !AutomationPredicate.root(target))) {
            target = target.parent;
          }
        } else {
          // Search for a root window with a title.
          while (target) {
            const isNamedWindow =
                !!target.name && target.role === RoleType.WINDOW;
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
        } else {
          output.withString(target.name);
        }

        output.go();
      }
        return false;
      case 'readCurrentURL':
        const output = new Output();
        const target = ChromeVoxState.instance.currentRange.start.node.root;
        output.withString(target.docUrl || '').go();
        return false;
      case 'toggleSelection':
        if (!ChromeVoxState.instance.pageSel_) {
          ChromeVoxState.instance.pageSel_ =
              ChromeVoxState.instance.currentRange;
          DesktopAutomationInterface.instance.ignoreDocumentSelectionFromAction(
              true);
        } else {
          const root = ChromeVoxState.instance.currentRange.start.node.root;
          if (root && root.selectionStartObject && root.selectionEndObject) {
            const sel = new cursors.Range(
                new cursors.Cursor(
                    root.selectionStartObject, root.selectionStartOffset),
                new cursors.Cursor(
                    root.selectionEndObject, root.selectionEndOffset));
            const o =
                new Output()
                    .format('@end_selection')
                    .withSpeechAndBraille(sel, sel, OutputEventType.NAVIGATE)
                    .go();
            DesktopAutomationInterface.instance
                .ignoreDocumentSelectionFromAction(false);
          }
          ChromeVoxState.instance.pageSel_ = null;
          return false;
        }
        break;
      case 'fullyDescribe':
        const o = new Output();
        o.withContextFirst()
            .withRichSpeechAndBraille(current, null, OutputEventType.NAVIGATE)
            .go();
        return false;
      case 'viewGraphicAsBraille':
        this.viewGraphicAsBraille_(current);
        return false;
      // Table commands.
      case 'previousRow': {
        skipSync = true;
        dir = Dir.BACKWARD;
        const tableOpts = {row: true, dir};
        pred = AutomationPredicate.makeTableCellPredicate(
            current.start.node, tableOpts);
        predErrorMsg = 'no_cell_above';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
      } break;
      case 'previousCol': {
        skipSync = true;
        dir = Dir.BACKWARD;
        const tableOpts = {col: true, dir};
        pred = AutomationPredicate.makeTableCellPredicate(
            current.start.node, tableOpts);
        predErrorMsg = 'no_cell_left';
        rootPred = AutomationPredicate.row;
        shouldWrap = false;
      } break;
      case 'nextRow': {
        skipSync = true;
        const tableOpts = {row: true, dir};
        pred = AutomationPredicate.makeTableCellPredicate(
            current.start.node, tableOpts);
        predErrorMsg = 'no_cell_below';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
      } break;
      case 'nextCol': {
        skipSync = true;
        const tableOpts = {col: true, dir};
        pred = AutomationPredicate.makeTableCellPredicate(
            current.start.node, tableOpts);
        predErrorMsg = 'no_cell_right';
        rootPred = AutomationPredicate.row;
        shouldWrap = false;
      } break;
      case 'goToRowFirstCell':
      case 'goToRowLastCell': {
        skipSync = true;
        while (node && node.role !== RoleType.ROW) {
          node = node.parent;
        }
        if (!node) {
          break;
        }
        const end = AutomationUtil.findNodePost(
            node, command === 'goToRowLastCell' ? Dir.BACKWARD : Dir.FORWARD,
            AutomationPredicate.leaf);
        if (end) {
          current = cursors.Range.fromNode(end);
        }
      } break;
      case 'goToColFirstCell': {
        skipSync = true;
        while (node && node.role !== RoleType.TABLE) {
          node = node.parent;
        }
        if (!node || !node.firstChild) {
          return false;
        }
        const tableOpts = {col: true, dir, end: true};
        pred = AutomationPredicate.makeTableCellPredicate(
            current.start.node, tableOpts);
        current = cursors.Range.fromNode(node.firstChild);
        // Should not be outputted.
        predErrorMsg = 'no_cell_above';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
      } break;
      case 'goToColLastCell': {
        skipSync = true;
        dir = Dir.BACKWARD;
        while (node && node.role !== RoleType.TABLE) {
          node = node.parent;
        }
        if (!node || !node.lastChild) {
          return false;
        }
        const tableOpts = {col: true, dir, end: true};
        pred = AutomationPredicate.makeTableCellPredicate(
            current.start.node, tableOpts);

        // Try to start on the last cell of the table and allow
        // matching that node.
        let startNode = node.lastChild;
        while (startNode.lastChild &&
               !AutomationPredicate.cellLike(startNode.role)) {
          startNode = startNode.lastChild;
        }
        current = cursors.Range.fromNode(startNode);
        matchCurrent = true;

        // Should not be outputted.
        predErrorMsg = 'no_cell_below';
        rootPred = AutomationPredicate.table;
        shouldWrap = false;
      } break;
      case 'goToFirstCell':
      case 'goToLastCell': {
        skipSync = true;
        while (node && node.role !== RoleType.TABLE) {
          node = node.parent;
        }
        if (!node) {
          break;
        }
        const end = AutomationUtil.findNodePost(
            node, command === 'goToLastCell' ? Dir.BACKWARD : Dir.FORWARD,
            AutomationPredicate.leaf);
        if (end) {
          current = cursors.Range.fromNode(end);
        }
      } break;

      // These commands are only available when invoked from touch.
      case 'nextAtGranularity':
      case 'previousAtGranularity':
        const backwards = command === 'previousAtGranularity';
        switch (GestureInterface.getGranularity()) {
          case GestureGranularity.CHARACTER:
            command = backwards ? 'previousCharacter' : 'nextCharacter';
            break;
          case GestureGranularity.WORD:
            command = backwards ? 'previousWord' : 'nextWord';
            break;
          case GestureGranularity.LINE:
            command = backwards ? 'previousLine' : 'nextLine';
            break;
          case GestureGranularity.HEADING:
            command = backwards ? 'previousHeading' : 'nextHeading';
            break;
          case GestureGranularity.LINK:
            command = backwards ? 'previousLink' : 'nextLink';
            break;
          case GestureGranularity.FORM_FIELD_CONTROL:
            command = backwards ? 'previousFormField' : 'nextFormField';
            break;
        }
        this.onCommand(command);
        return false;
      case 'announceRichTextDescription': {
        const optSubs = [];
        node.fontSize ? optSubs.push('font size: ' + node.fontSize) :
                        optSubs.push('');
        node.color ? optSubs.push(Color.getColorDescription(node.color)) :
                     optSubs.push('');
        node.bold ? optSubs.push(Msgs.getMsg('bold')) : optSubs.push('');
        node.italic ? optSubs.push(Msgs.getMsg('italic')) : optSubs.push('');
        node.underline ? optSubs.push(Msgs.getMsg('underline')) :
                         optSubs.push('');
        node.lineThrough ? optSubs.push(Msgs.getMsg('linethrough')) :
                           optSubs.push('');
        node.fontFamily ? optSubs.push('font family: ' + node.fontFamily) :
                          optSubs.push('');

        const richTextDescription =
            Msgs.getMsg('rich_text_attributes', optSubs);
        new Output()
            .withString(richTextDescription)
            .withQueueMode(QueueMode.CATEGORY_FLUSH)
            .go();
      }
        return false;
      case 'readPhoneticPronunciation': {
        // Get node info.
        const index = ChromeVoxState.instance.currentRange.start.index;
        const name = node.name;
        // If there is no text to speak, inform the user and return early.
        if (!name) {
          new Output()
              .withString(Msgs.getMsg('empty_name'))
              .withQueueMode(QueueMode.CATEGORY_FLUSH)
              .go();
          return false;
        }

        // Get word start and end indices.
        let wordStarts, wordEnds;
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
        return false;
      case 'readLinkURL': {
        const rootNode = node.root;
        while (node && !node.url) {
          // URL could be an ancestor of current range.
          node = node.parent;
        }
        // Announce node's URL if it's not the root node; we don't want to
        // announce the URL of the current page.
        const url = (node && node !== rootNode) ? node.url : '';
        new Output()
            .withString(
                url ? Msgs.getMsg('url_behind_link', [url]) :
                      Msgs.getMsg('no_url_found'))
            .withQueueMode(QueueMode.CATEGORY_FLUSH)
            .go();
      }
        return false;
      case 'logLanguageInformationForCurrentNode': {
        if (!this.languageLoggingEnabled_) {
          return false;
        }

        const outString = `
      Language information for node
      Name: ${node.name}
      Detected language: ${node.detectedLanguage || 'None'}
      Author language: ${node.language || 'None'}
      `;
        new Output()
            .withString(outString)
            .withQueueMode(QueueMode.CATEGORY_FLUSH)
            .go();
        const annotation = node.languageAnnotationForStringAttribute('name');
        const logString = outString.concat(`Language spans:
        ${JSON.stringify(annotation)}`);
        console.error(logString);
        LogStore.getInstance().writeTextLog(logString, LogStore.LogType.TEXT);
      }
        return false;
      default:
        return true;
    }

    if (didNavigate) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.ChromeVox.Navigate');
    }

    if (pred) {
      chrome.metricsPrivate.recordUserAction('Accessibility.ChromeVox.Jump');

      let bound = current.getBound(dir).node;
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
          current = cursors.Range.fromNode(node);
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
                        root, dir, AutomationPredicate.leaf) ||
                bound;
          }
          node =
              AutomationUtil.findNextNode(bound, dir, pred, {root: rootPred});

          if (node && !skipSync) {
            node = AutomationUtil.findNodePre(
                       node, Dir.FORWARD, AutomationPredicate.object) ||
                node;
          }

          if (node) {
            current = cursors.Range.fromNode(node);
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

    if (tryScrolling &&
        !AutoScrollHandler.getInstance().onCommandNavigation(
            current, dir, pred, unit, speechProps, rootPred, () => {
              this.onCommand(command);
              this.onFinishCommand();
            })) {
      this.onFinishCommand();
      return false;
    }

    if (current) {
      if (current.wrapped) {
        ChromeVox.earcons.playEarcon(Earcon.WRAP);
      }

      ChromeVoxState.instance.navigateToRange(
          current, undefined, speechProps, skipSettingSelection);
    }

    this.onFinishCommand();
    return false;
  }

  /**
   * Finishes processing of a command.
   */
  onFinishCommand() {
    this.smartStickyMode_.stopIgnoringRangeChanges();
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
   * @param {!(AutomationEvent|CustomAutomationEvent)} event The event.
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
   * @param {!cursors.Range} current The current range.
   * @private
   */
  viewGraphicAsBraille_(current) {
    if (this.imageNode_) {
      this.imageNode_.removeEventListener(
          EventType.IMAGE_FRAME_UPDATED, this.onImageFrameUpdated_, false);
      this.imageNode_ = null;
    }

    // Find the first node within the current range that supports image data.
    const imageNode = AutomationUtil.findNodePost(
        current.start.node, Dir.FORWARD, AutomationPredicate.supportsImageData);
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
   * @param {string} command
   * @return {boolean} True if the command should propagate.
   * @private
   */
  onEditCommand_(command) {
    if (ChromeVox.isStickyModeOn()) {
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
      case 'previousCharacter':
        EventGenerator.sendKeyPress(KeyCode.HOME, {shift: true});
        break;
      case 'nextCharacter':
        EventGenerator.sendKeyPress(KeyCode.END, {shift: true});
        break;
      case 'previousWord':
        EventGenerator.sendKeyPress(KeyCode.HOME, {shift: true, ctrl: true});
        break;
      case 'nextWord':
        EventGenerator.sendKeyPress(KeyCode.END, {shift: true, ctrl: true});
        break;
      case 'previousObject':
        if (!isMultiline) {
          return true;
        }

        if (textEditHandler.isSelectionOnFirstLine()) {
          ChromeVoxState.instance.setCurrentRange(
              cursors.Range.fromNode(textEditHandler.node));
          return true;
        }
        EventGenerator.sendKeyPress(KeyCode.HOME);
        break;
      case 'nextObject':
        if (!isMultiline) {
          return true;
        }

        if (textEditHandler.isSelectionOnLastLine()) {
          textEditHandler.moveToAfterEditText();
          return false;
        }

        EventGenerator.sendKeyPress(KeyCode.END);
        break;
      case 'previousLine':
        if (!isMultiline) {
          return true;
        }
        if (textEditHandler.isSelectionOnFirstLine()) {
          ChromeVoxState.instance.setCurrentRange(
              cursors.Range.fromNode(textEditHandler.node));
          return true;
        }
        EventGenerator.sendKeyPress(KeyCode.PRIOR);
        break;
      case 'nextLine':
        if (!isMultiline) {
          return true;
        }

        if (textEditHandler.isSelectionOnLastLine()) {
          textEditHandler.moveToAfterEditText();
          return false;
        }
        EventGenerator.sendKeyPress(KeyCode.NEXT);
        break;
      case 'jumpToTop':
        EventGenerator.sendKeyPress(KeyCode.HOME, {ctrl: true});
        break;
      case 'jumpToBottom':
        EventGenerator.sendKeyPress(KeyCode.END, {ctrl: true});
        break;
      default:
        return true;
    }
    return false;
  }

  /** @override */

  skipLabelOrDescriptionFor(current, dir) {
    if (!current) {
      return null;
    }

    // Keep moving past all nodes acting as labels or descriptions.
    while (current && current.start && current.start.node &&
           current.start.node.role === RoleType.STATIC_TEXT) {
      // We must scan upwards as any ancestor might have a label or description.
      let ancestor = current.start.node;
      while (ancestor) {
        if ((ancestor.labelFor && ancestor.labelFor.length > 0) ||
            (ancestor.descriptionFor && ancestor.descriptionFor.length > 0)) {
          break;
        }
        ancestor = ancestor.parent;
      }
      if (ancestor) {
        current = current.move(cursors.Unit.NODE, dir);
      } else {
        break;
      }
    }

    return current;
  }

  /**
   * Performs global initialization.
   */
  init() {
    ChromeVoxKbHandler.commandHandler = this.onCommand.bind(this);

    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-language-detection', (enabled) => {
          if (enabled) {
            this.languageLoggingEnabled_ = true;
          }
        });
    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-language-detection-dynamic',
        (enabled) => {
          if (enabled) {
            this.languageLoggingEnabled_ = true;
          }
        });

    chrome.chromeosInfoPrivate.get(['sessionType'], (result) => {
      /** @type {boolean} */
      this.isKioskSession_ = result['sessionType'] ===
          chrome.chromeosInfoPrivate.SessionType.KIOSK;
    });
  }
}

CommandHandlerInterface.instance = new CommandHandler();
