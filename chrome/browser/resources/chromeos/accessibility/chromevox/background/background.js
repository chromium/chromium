// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '../../common/automation_predicate.js';
import {AutomationUtil} from '../../common/automation_util.js';
import {constants} from '../../common/constants.js';
import {CursorRange} from '../../common/cursors/range.js';
import {InstanceChecker} from '../../common/instance_checker.js';
import {LocalStorage} from '../../common/local_storage.js';
import {AbstractEarcons} from '../common/abstract_earcons.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {ContentScriptBridge} from '../common/content_script_bridge.js';
import {LocaleOutputHelper} from '../common/locale_output_helper.js';
import {Msgs} from '../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';
import {JaPhoneticMap} from '../third_party/tamachiyomi/ja_phonetic_map.js';

import {AutoScrollHandler} from './auto_scroll_handler.js';
import {BrailleBackground} from './braille/braille_background.js';
import {BrailleCommandHandler} from './braille/braille_command_handler.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxState} from './chromevox_state.js';
import {ChromeVoxBackground} from './classic_background.js';
import {ClipboardHandler} from './clipboard_handler.js';
import {CommandHandler} from './command_handler.js';
import {DesktopAutomationHandler} from './desktop_automation_handler.js';
import {DesktopAutomationInterface} from './desktop_automation_interface.js';
import {DownloadHandler} from './download_handler.js';
import {Earcons} from './earcons.js';
import {FindHandler} from './find_handler.js';
import {FocusAutomationHandler} from './focus_automation_handler.js';
import {FocusBounds} from './focus_bounds.js';
import {GestureCommandHandler} from './gesture_command_handler.js';
import {BackgroundKeyboardHandler} from './keyboard_handler.js';
import {LiveRegions} from './live_regions.js';
import {EventStreamLogger} from './logging/event_stream_logger.js';
import {LogStore} from './logging/log_store.js';
import {MathHandler} from './math_handler.js';
import {MediaAutomationHandler} from './media_automation_handler.js';
import {Output} from './output/output.js';
import {OutputCustomEvent} from './output/output_types.js';
import {PageLoadSoundHandler} from './page_load_sound_handler.js';
import {PanelBackground} from './panel/panel_background.js';
import {ChromeVoxPrefs} from './prefs.js';
import {RangeAutomationHandler} from './range_automation_handler.js';
import {TtsBackground} from './tts_background.js';

/**
 * @fileoverview The entry point for all ChromeVox related code for the
 * background page.
 */

const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;
/** ChromeVox background page. */
export class Background extends ChromeVoxState {
  constructor() {
    super();

    /** @private {CursorRange} */
    this.currentRange_ = null;

    /** @private {!AbstractEarcons} */
    this.earcons_ = new Earcons();

    /** @private {boolean} */
    this.isReadingContinuously_ = false;

    /** @private {CursorRange} */
    this.pageSel_ = null;

    /** @private {CursorRange} */
    this.previousRange_ = null;

    /** @private {boolean} */
    this.talkBackEnabled_ = false;

    this.init_();
  }

  /** @private */
  init_() {
    chrome.accessibilityPrivate.onIntroduceChromeVox.addListener(
        () => this.onIntroduceChromeVox_());

    // Export globals on ChromeVox.
    ChromeVox.braille = BrailleBackground.instance;
    // Read-only earcons.
    Object.defineProperty(ChromeVox, 'earcons', {
      get: () => this.earcons_,
    });

    chrome.accessibilityPrivate.onAnnounceForAccessibility.addListener(
        announceText => {
          ChromeVox.tts.speak(announceText.join(' '), QueueMode.FLUSH);
        });
    chrome.accessibilityPrivate.onCustomSpokenFeedbackToggled.addListener(
        enabled => this.talkBackEnabled_ = enabled);
    chrome.accessibilityPrivate.onIntroduceChromeVox.addListener(
        () => this.onIntroduceChromeVox_());
    chrome.accessibilityPrivate.onShowChromeVoxTutorial.addListener(() => {
      (new PanelCommand(PanelCommandType.TUTORIAL)).send();
    });
  }

  static async init() {
    // Initialize storage, braille, prefs, TTS, and legacy background page
    // first.
    await LocalStorage.init();
    BrailleBackground.init();
    ChromeVoxPrefs.init();
    TtsBackground.init();
    ChromeVoxBackground.init();

    ChromeVoxState.instance = new Background();

    AutoScrollHandler.init();
    BackgroundKeyboardHandler.init();
    BrailleCommandHandler.init();
    ClipboardHandler.init();
    CommandHandler.init();
    DownloadHandler.init();
    EventStreamLogger.init();
    FindHandler.init();
    FocusAutomationHandler.init();
    GestureCommandHandler.init();
    JaPhoneticData.init(JaPhoneticMap.MAP);
    LiveRegions.init();
    LocaleOutputHelper.init();
    LogStore.init();
    MediaAutomationHandler.init();
    PageLoadSoundHandler.init();
    PanelBackground.init();
    RangeAutomationHandler.init();

    // Allow all async initializers to run simultaneously, but wait for them to
    // complete before continuing.
    await Promise.all([
      DesktopAutomationHandler.init(),
    ]);
    ChromeVoxState.resolveReadyPromise_();
  }

  /** @override */
  getCurrentRange() {
    if (this.currentRange_ && this.currentRange_.isValid()) {
      return this.currentRange_;
    }
    return null;
  }

  /** @override */
  get isReadingContinuously() {
    return this.isReadingContinuously_;
  }

  /** @override */
  get pageSel() {
    return this.pageSel_;
  }

  /** @override */
  get talkBackEnabled() {
    return this.talkBackEnabled_;
  }

  /** @override */
  getCurrentRangeWithoutRecovery() {
    return this.currentRange_;
  }

  /**
   * @param {CursorRange} newRange The new range.
   * @param {boolean=} opt_fromEditing
   * @override
   */
  setCurrentRange(newRange, opt_fromEditing) {
    // Clear anything that was frozen on the braille display whenever
    // the user navigates.
    ChromeVox.braille.thaw();

    // There's nothing to be updated in this case.
    if ((!newRange && !this.currentRange_) ||
        (newRange && !newRange.isValid())) {
      FocusBounds.set([]);
      return;
    }

    this.previousRange_ = this.currentRange_;
    this.currentRange_ = newRange;

    ChromeVoxState.ready().then(ChromeVoxState.observers.forEach(
        observer => observer.onCurrentRangeChanged(newRange, opt_fromEditing)));

    if (!this.currentRange_) {
      FocusBounds.set([]);
      return;
    }

    const start = this.currentRange_.start.node;
    start.makeVisible();
    start.setAccessibilityFocus();

    const root = AutomationUtil.getTopLevelRoot(start);
    if (!root || root.role === RoleType.DESKTOP || root === start) {
      return;
    }

    const position = {};
    const loc = start.unclippedLocation;
    position.x = loc.left + loc.width / 2;
    position.y = loc.top + loc.height / 2;
    let url = root.docUrl;
    url = url.substring(0, url.indexOf('#')) || url;
    ChromeVoxState.position[url] = position;
  }

  /** @override */
  set isReadingContinuously(newValue) {
    this.isReadingContinuously_ = newValue;
  }

  /** @override */
  set pageSel(newPageSel) {
    this.pageSel_ = newPageSel;
  }

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!CursorRange} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {TtsSpeechProperties=} opt_speechProps Speech properties.
   * @param {boolean=} opt_skipSettingSelection If true, does not set
   *     the selection, otherwise it does by default.
   * @override
   */
  navigateToRange(range, opt_focus, opt_speechProps, opt_skipSettingSelection) {
    opt_focus = opt_focus === undefined ? true : opt_focus;
    opt_speechProps = opt_speechProps || new TtsSpeechProperties();
    opt_skipSettingSelection = opt_skipSettingSelection || false;
    const prevRange = this.currentRange_;

    // Specialization for math output.
    let skipOutput = false;
    if (MathHandler.init(range)) {
      skipOutput = MathHandler.instance.speak();
      opt_focus = false;
    }

    if (opt_focus) {
      this.setFocusToRange_(range, prevRange);
    }

    this.setCurrentRange(range);

    const o = new Output();
    let selectedRange;
    let msg;

    if (this.pageSel_ && this.pageSel_.isValid() && range.isValid()) {
      // Suppress hints.
      o.withoutHints();

      // Selection across roots isn't supported.
      const pageRootStart = this.pageSel_.start.node.root;
      const pageRootEnd = this.pageSel_.end.node.root;
      const curRootStart = range.start.node.root;
      const curRootEnd = range.end.node.root;

      // Deny crossing over the start of the page selection and roots.
      if (pageRootStart !== pageRootEnd || pageRootStart !== curRootStart ||
          pageRootEnd !== curRootEnd) {
        o.format('@end_selection');
        DesktopAutomationInterface.instance.ignoreDocumentSelectionFromAction(
            false);
        this.pageSel_ = null;
      } else {
        // Expand or shrink requires different feedback.

        // Page sel is the only place in ChromeVox where we used directed
        // selections. It is important to keep track of the directedness in
        // places, but when comparing to other ranges, take the undirected
        // range.
        const dir = this.pageSel_.normalize().compare(range);

        if (dir) {
          // Directed expansion.
          msg = '@selected';
        } else {
          // Directed shrink.
          msg = '@unselected';
          selectedRange = prevRange;
        }
        const wasBackwardSel =
            this.pageSel_.start.compare(this.pageSel_.end) === Dir.BACKWARD ||
            dir === Dir.BACKWARD;
        this.pageSel_ = new CursorRange(
            this.pageSel_.start, wasBackwardSel ? range.start : range.end);
        if (this.pageSel_) {
          this.pageSel_.select();
        }
      }
    } else if (!opt_skipSettingSelection) {
      // Ensure we don't select the editable when we first encounter it.
      let lca = null;
      if (range.start.node && prevRange.start.node) {
        lca = AutomationUtil.getLeastCommonAncestor(
            prevRange.start.node, range.start.node);
      }
      if (!lca || lca.state[StateType.EDITABLE] ||
          !range.start.node.state[StateType.EDITABLE]) {
        range.select();
      }
    }

    o.withRichSpeechAndBraille(
         selectedRange || range, prevRange, OutputCustomEvent.NAVIGATE)
        .withInitialSpeechProperties(opt_speechProps);

    if (msg) {
      o.format(msg);
    }

    if (!skipOutput) {
      o.go();
    }
  }

  /** @override */
  onBrailleKeyEvent(evt, content) {
    return BrailleCommandHandler.onBrailleKeyEvent(evt, content);
  }

  /** @override */
  restoreLastValidRangeIfNeeded() {
    // Never restore range when TalkBack is enabled as commands such as
    // Search+Left, go directly to TalkBack.
    if (this.talkBackEnabled) {
      return;
    }

    if (!this.currentRange_ || !this.currentRange_.isValid()) {
      this.setCurrentRange(this.previousRange_);
    }
  }

  /** @private */
  async setCurrentRangeToFocus_() {
    const focus =
        await new Promise(resolve => chrome.automation.getFocus(resolve));
    if (focus) {
      this.setCurrentRange(CursorRange.fromNode(focus));
    } else {
      this.setCurrentRange(null);
    }
  }

  /**
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @private
   */
  setFocusToRange_(range, prevRange) {
    const start = range.start.node;
    const end = range.end.node;

    // First, see if we've crossed a root. Remove once webview handles focus
    // correctly.
    if (prevRange && prevRange.start.node && start) {
      const entered =
          AutomationUtil.getUniqueAncestors(prevRange.start.node, start);

      entered
          .filter(
              ancestor => ancestor.role === RoleType.PLUGIN_OBJECT ||
                  ancestor.role === RoleType.IFRAME)
          .forEach(container => {
            if (!container.state[StateType.FOCUSED]) {
              container.focus();
            }
          });
    }

    if (start.state[StateType.FOCUSED] || end.state[StateType.FOCUSED]) {
      return;
    }

    const isFocusableLinkOrControl = node => node.state[StateType.FOCUSABLE] &&
        AutomationPredicate.linkOrControl(node);

    // Next, try to focus the start or end node.
    if (!AutomationPredicate.structuralContainer(start) &&
        start.state[StateType.FOCUSABLE]) {
      if (!start.state[StateType.FOCUSED]) {
        start.focus();
      }
      return;
    } else if (
        !AutomationPredicate.structuralContainer(end) &&
        end.state[StateType.FOCUSABLE]) {
      if (!end.state[StateType.FOCUSED]) {
        end.focus();
      }
      return;
    }

    // If a common ancestor of |start| and |end| is a link, focus that.
    let ancestor = AutomationUtil.getLeastCommonAncestor(start, end);
    while (ancestor && ancestor.root === start.root) {
      if (isFocusableLinkOrControl(ancestor)) {
        if (!ancestor.state[StateType.FOCUSED]) {
          ancestor.focus();
        }
        return;
      }
      ancestor = ancestor.parent;
    }

    // If nothing is focusable, set the sequential focus navigation starting
    // point, which ensures that the next time you press Tab, you'll reach
    // the next or previous focusable node from |start|.
    if (!start.state[StateType.OFFSCREEN]) {
      start.setSequentialFocusNavigationStartingPoint();
    }
  }

  /**
   * Handles the onIntroduceChromeVox event.
   * @private
   */
  onIntroduceChromeVox_() {
    ChromeVox.tts.speak(
        Msgs.getMsg('chromevox_intro'), QueueMode.QUEUE,
        new TtsSpeechProperties({doNotInterrupt: true}));
    ChromeVox.braille.write(NavBraille.fromText(Msgs.getMsg('intro_brl')));
  }

  /**
   * Converts a list of globs, as used in the extension manifest, to a regular
   * expression that matches if and only if any of the globs in the list
   * matches.
   * @param {!Array<string>} globs
   * @return {!RegExp}
   * @private
   */
  static globsToRegExp_(globs) {
    return new RegExp(
        '^(' +
        globs
            .map(
                glob => glob.replace(/[.+^$(){}|[\]\\]/g, '\\$&')
                            .replace(/\*/g, '.*')
                            .replace(/\?/g, '.'))
            .join('|') +
        ')$');
  }
}

InstanceChecker.closeExtraInstances();
Background.init();
