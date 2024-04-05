// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../common/learn_mode_bridge.js';

import {Flags} from '/common/flags.js';
import {InstanceChecker} from '/common/instance_checker.js';
import {LocalStorage} from '/common/local_storage.js';

import {BrailleKeyEvent} from '../common/braille/braille_key_types.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {EarconId} from '../common/earcon_id.js';
import {LocaleOutputHelper} from '../common/locale_output_helper.js';
import {Msgs} from '../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {PermissionChecker} from '../common/permission_checker.js';
import {SettingsManager} from '../common/settings_manager.js';
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';
import {JaPhoneticData} from '../third_party/tamachiyomi/ja_phonetic_data.js';
import {JaPhoneticMap} from '../third_party/tamachiyomi/ja_phonetic_map.js';

import {AbstractEarcons} from './abstract_earcons.js';
import {AutoScrollHandler} from './auto_scroll_handler.js';
import {BrailleBackground} from './braille/braille_background.js';
import {BrailleCommandHandler} from './braille/braille_command_handler.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxRange} from './chromevox_range.js';
import {ChromeVoxState} from './chromevox_state.js';
import {DownloadHandler} from './download_handler.js';
import {Earcons} from './earcons.js';
import {EditingRangeObserver} from './editing/editing_range_observer.js';
import {DesktopAutomationHandler} from './event/desktop_automation_handler.js';
import {FocusAutomationHandler} from './event/focus_automation_handler.js';
import {MediaAutomationHandler} from './event/media_automation_handler.js';
import {PageLoadSoundHandler} from './event/page_load_sound_handler.js';
import {RangeAutomationHandler} from './event/range_automation_handler.js';
import {EventSource} from './event_source.js';
import {FindHandler} from './find_handler.js';
import {InjectedScriptLoader} from './injected_script_loader.js';
import {BackgroundKeyboardHandler} from './input/background_keyboard_handler.js';
import {ClipboardHandler} from './input/clipboard_handler.js';
import {CommandHandler} from './input/command_handler.js';
import {GestureCommandHandler} from './input/gesture_command_handler.js';
import {SmartStickyMode} from './input/smart_sticky_mode.js';
import {LiveRegions} from './live_regions.js';
import {EventStreamLogger} from './logging/event_stream_logger.js';
import {LogStore} from './logging/log_store.js';
import {LogUrlWatcher} from './logging/log_url_watcher.js';
import {PanelBackground} from './panel/panel_background.js';
import {ChromeVoxPrefs} from './prefs.js';
import {TtsBackground} from './tts_background.js';

/**
 * @fileoverview The entry point for all ChromeVox related code for the
 * background page.
 */

/** ChromeVox background context. */
export class Background extends ChromeVoxState {
  private startUpTime_ = new Date().getTime();
  private earcons_: AbstractEarcons;
  private isReadingContinuously_ = false;
  private talkBackEnabled_ = false;

  constructor() {
    super();

    this.earcons_ = new Earcons();
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

  static async init(): Promise<void> {
    // Pre-initialization.
    await Flags.init();
    await LocalStorage.init();
    await SettingsManager.init();
    BrailleBackground.init();
    ChromeVoxPrefs.init();
    ChromeVoxRange.init();
    TtsBackground.init();
    LogStore.init();

    ChromeVoxState.instance = new Background();

    // Standard initialization.
    AutoScrollHandler.init();
    BackgroundKeyboardHandler.init();
    BrailleCommandHandler.init();
    ClipboardHandler.init();
    CommandHandler.init();
    DownloadHandler.init();
    EditingRangeObserver.init();
    EventSource.init();
    FindHandler.init();
    GestureCommandHandler.init();
    InjectedScriptLoader.injectContentScriptForGoogleDocs();
    JaPhoneticData.init(JaPhoneticMap.MAP);
    LiveRegions.init();
    LocaleOutputHelper.init();
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
    ChromeVoxState.instance.onIntroduceChromeVox();
  }

  override get isReadingContinuously(): boolean {
    return this.isReadingContinuously_;
  }

  override get talkBackEnabled(): boolean {
    return this.talkBackEnabled_;
  }

  override set isReadingContinuously(newValue: boolean) {
    this.isReadingContinuously_ = newValue;
  }

  override onBrailleKeyEvent(
      evt: BrailleKeyEvent, content: NavBraille): boolean {
    return BrailleCommandHandler.onBrailleKeyEvent(evt, content);
  }

  /** Handles the onIntroduceChromeVox event. */
  override onIntroduceChromeVox(): void {
    this.earcons_.playEarcon(EarconId.CHROMEVOX_LOADED);
    ChromeVox.tts.speak(
        Msgs.getMsg('chromevox_intro'), QueueMode.QUEUE,
        new TtsSpeechProperties({
          doNotInterrupt: true,
          startCallback: () => {
            chrome.metricsPrivate.recordMediumTime(
                'Accessibility.ChromeVox.StartUpSpeechDelay',
                new Date().getTime() - this.startUpTime_);
          },
        }));
    ChromeVox.braille.write(NavBraille.fromText(Msgs.getMsg('intro_brl')));
  }
}

InstanceChecker.closeExtraInstances();
const waitForIntroducePromise = new Promise(
    (resolve: (value: unknown) => void) =>
        chrome.accessibilityPrivate.onIntroduceChromeVox.addListener(
            resolve as VoidFunction));
Background.init();
