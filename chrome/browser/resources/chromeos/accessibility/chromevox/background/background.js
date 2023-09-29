// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '../../common/automation_predicate.js';
import {AutomationUtil} from '../../common/automation_util.js';
import {CursorRange} from '../../common/cursors/range.js';
import {Flags} from '../../common/flags.js';
import {InstanceChecker} from '../../common/instance_checker.js';
import {LocalStorage} from '../../common/local_storage.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {EarconId} from '../common/earcon_id.js';
import {LocaleOutputHelper} from '../common/locale_output_helper.js';
import {Msgs} from '../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {PermissionChecker} from '../common/permission_checker.js';
import {SettingsManager} from '../common/settings_manager.js';
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';
import {JaPhoneticMap} from '../third_party/tamachiyomi/ja_phonetic_map.js';

import {AbstractEarcons} from './abstract_earcons.js';
import {AutoScrollHandler} from './auto_scroll_handler.js';
import {BrailleBackground} from './braille/braille_background.js';
import {BrailleCommandHandler} from './braille/braille_command_handler.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxRange} from './chromevox_range.js';
import {ChromeVoxState} from './chromevox_state.js';
import {ChromeVoxBackground} from './classic_background.js';
import {ClipboardHandler} from './clipboard_handler.js';
import {CommandHandler} from './command_handler.js';
import {DownloadHandler} from './download_handler.js';
import {Earcons} from './earcons.js';
import {DesktopAutomationHandler} from './event/desktop_automation_handler.js';
import {FocusAutomationHandler} from './event/focus_automation_handler.js';
import {MediaAutomationHandler} from './event/media_automation_handler.js';
import {PageLoadSoundHandler} from './event/page_load_sound_handler.js';
import {RangeAutomationHandler} from './event/range_automation_handler.js';
import {EventSource} from './event_source.js';
import {FindHandler} from './find_handler.js';
import {GestureCommandHandler} from './gesture_command_handler.js';
import {BackgroundKeyboardHandler} from './keyboard_handler.js';
import {LiveRegions} from './live_regions.js';
import {EventStreamLogger} from './logging/event_stream_logger.js';
import {LogStore} from './logging/log_store.js';
import {LogUrlWatcher} from './logging/log_url_watcher.js';
import {PanelBackground} from './panel/panel_background.js';
import {ChromeVoxPrefs} from './prefs.js';
import {SmartStickyMode} from './smart_sticky_mode.js';
import {TtsBackground} from './tts_background.js';

/**
 * @fileoverview The entry point for all ChromeVox related code for the
 * background page.
 */
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/** ChromeVox background context. */
export class Background extends ChromeVoxState {
  constructor() {
    super();

    /** @private {!AbstractEarcons} */
    this.earcons_ = new Earcons();

    /** @private {boolean} */
    this.isReadingContinuously_ = false;

    /** @private {CursorRange} */
    this.pageSel_ = null;

    /** @private {boolean} */
    this.talkBackEnabled_ = false;

    this.init_();
  }

  /** @private */
  init_() {
    this.earcons_.playEarcon(EarconId.CHROMEVOX_LOADING);

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
    chrome.accessibilityPrivate.onShowChromeVoxTutorial.addListener(() => {
      (new PanelCommand(PanelCommandType.TUTORIAL)).send();
    });
  }

  static async init() {
    // Pre-initialization.
    await Flags.init();
    await LocalStorage.init();
    await SettingsManager.init();
    BrailleBackground.init();
    ChromeVoxPrefs.init();
    ChromeVoxRange.init();
    TtsBackground.init();
    ChromeVoxBackground.init();

    ChromeVoxState.instance = new Background();

    // Standard initialization.
    AutoScrollHandler.init();
    BackgroundKeyboardHandler.init();
    BrailleCommandHandler.init();
    ClipboardHandler.init();
    CommandHandler.init();
    DownloadHandler.init();
    EventSource.init();
    FindHandler.init();
    GestureCommandHandler.init();
    JaPhoneticData.init(JaPhoneticMap.MAP);
    LiveRegions.init();
    LocaleOutputHelper.init();
    LogStore.init();
    LogUrlWatcher.init();
    PanelBackground.init();
    RangeAutomationHandler.init();
    SmartStickyMode.init();

    // Async initialization.
    // Allow all async initializers to run simultaneously, but wait for them to
    // complete before continuing.
    // The order that these are run in is not guaranteed.
    await Promise.all([
      DesktopAutomationHandler.init(),
      EventStreamLogger.init(),
      FocusAutomationHandler.init(),
      MediaAutomationHandler.init(),
      PageLoadSoundHandler.init(),
      PermissionChecker.init(),
      waitForIntroducePromise,
    ]);
    ChromeVoxState.resolveReadyPromise_();
    ChromeVoxState.instance.onIntroduceChromeVox_();
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
  set isReadingContinuously(newValue) {
    this.isReadingContinuously_ = newValue;
  }

  /** @override */
  set pageSel(newPageSel) {
    this.pageSel_ = newPageSel;
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

    if (!ChromeVoxRange.getCurrentRangeWithoutRecovery()?.isValid()) {
      ChromeVoxRange.set(ChromeVoxRange.previous);
    }
  }

  /**
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @override
   */
  setFocusToRange(range, prevRange) {
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
    this.earcons_.playEarcon(EarconId.CHROMEVOX_LOADED);
    ChromeVox.tts.speak(
        Msgs.getMsg('chromevox_intro'), QueueMode.QUEUE,
        new TtsSpeechProperties({doNotInterrupt: true}));
    ChromeVox.braille.write(NavBraille.fromText(Msgs.getMsg('intro_brl')));
  }
}

InstanceChecker.closeExtraInstances();
const waitForIntroducePromise = new Promise(
    resolve =>
        chrome.accessibilityPrivate.onIntroduceChromeVox.addListener(resolve));
Background.init();
