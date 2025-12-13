// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SRE} from '/chromevox/mv3/third_party/sre/sre_browser.js';
import {BridgeHelper} from '/common/bridge_helper.js';

import {BackgroundBridge} from '../common/background_bridge.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {InternalKeyEvent} from '../common/internal_key_event.js';
import {LearnModeBridge} from '../common/learn_mode_bridge.js';
import type {ClipboardData, StateWithMaxCellHeight} from '../common/offscreen_bridge_constants.js';

import {EarconEngine} from './earcon_engine.js';
import {LibLouisWorker} from './liblouis_worker.js';

const TARGET = BridgeConstants.Offscreen.TARGET;
const Action = BridgeConstants.Offscreen.Action;

/**
 * Tracks the state of ChromeVox initialization.
 */
class OffscreenChromeVoxState {
  static instance?: OffscreenChromeVoxState;
  ready: boolean = false;

  constructor() {
    BridgeHelper.registerHandler(
        TARGET, Action.CHROMEVOX_READY, () => {this.ready = true});
  }

  static init(): void {
    if (OffscreenChromeVoxState.instance) {
      throw new Error(
          'Error: trying to create two instances of singleton ' +
          'OffscreenChromeVoxState.');
    }
    OffscreenChromeVoxState.instance = new OffscreenChromeVoxState();
  }

  static isReady(): boolean {
    return this.instance?.ready || false;
  };
}

/**
 * Handles keydown and keyup events when Learn Mode is initiated.
 */
class OffscreenLearnModeKeyboardHandler {
  static instance?: OffscreenLearnModeKeyboardHandler;

  constructor() {
    BridgeHelper.registerHandler(
        TARGET, Action.LEARN_MODE_REGISTER_LISTENERS,
        () => this.registerListeners_());
    BridgeHelper.registerHandler(
        TARGET, Action.LEARN_MODE_REMOVE_LISTENERS,
        () => this.removeListeners_());
  }

  static init(): void {
    if (OffscreenLearnModeKeyboardHandler.instance) {
      throw new Error(
          'Error: trying to create two instances of singleton ' +
          'OffscreenLearnModeKeyboardHandler.');
    }
    OffscreenLearnModeKeyboardHandler.instance =
        new OffscreenLearnModeKeyboardHandler();
  }

  private registerListeners_(): void {
    window.addEventListener('keydown', this.onKeyDown_, true);
    window.addEventListener('keyup', this.onKeyUp_, true);
    window.addEventListener('keypress', this.onKeyPress_, true);
  }

  private removeListeners_(): void {
    window.removeEventListener('keydown', this.onKeyDown_, true);
    window.removeEventListener('keyup', this.onKeyUp_, true);
    window.removeEventListener('keypress', this.onKeyPress_, true);
  }

  private onKeyDown_(evt: KeyboardEvent): void {
    const internalEvt = new InternalKeyEvent(evt);
    LearnModeBridge.onKeyDown(internalEvt).then((stopProp: boolean) => {
      if (stopProp) {
        evt.preventDefault();
        evt.stopPropagation();
      }
    });
  }

  private onKeyUp_(evt: KeyboardEvent): void {
    evt.preventDefault();
    evt.stopPropagation();

    LearnModeBridge.onKeyUp();
  }

  private onKeyPress_(evt: KeyboardEvent): void {
    evt.preventDefault();
    evt.stopPropagation();

    LearnModeBridge.onKeyPress();
  }
}


/**
 * Handles DOM interactions when accessing and tracking access to the clipboard,
 * used by ClipboardHandler instance within the service worker.
 */
class OffscreenClipboardHandler {
  static instance?: OffscreenClipboardHandler;
  private lastClipboardEvent_?: string;

  constructor() {
    document.addEventListener(
        'copy', event => this.onClipboardCopyEvent_(event));

    BridgeHelper.registerHandler(
        TARGET, Action.ON_CLIPBOARD_DATA_CHANGED,
        (forceRead: boolean) => this.onClipboardDataChanged_(forceRead));
  }

  static init(): void {
    if (OffscreenClipboardHandler.instance) {
      throw new Error(
          'Error: trying to create two instances of singleton ' +
          'OffscreenClipboardHandler.');
    }
    OffscreenClipboardHandler.instance = new OffscreenClipboardHandler();
  }

  /** Processes the copy clipboard event. */
  private onClipboardCopyEvent_(evt: ClipboardEvent): void {
    // This should always be 'copy', but is still important to set for the below
    // extension event.
    this.lastClipboardEvent_ = evt.type;
  }


  /**
   * Called in response to the chrome API call
   * chrome.clipboard.onClipboardDataChanged. There are two scenarios where the
   * most recently copied string should be processed and the text of the change
   * sent back to the service worker:
   * 1. Immediately after a DOM-based clipboard 'copy' event. A DOM-based
   * clipboard event always comes before the chrome API call, and that event is
   * listened for and sets this.lastClipboardEvent_ in
   * OffscreenClipboardHandler.
   * 2. When the 'forceRead' argument is true. 'forceRead' is set in response to
   * the call to ClipboardHandler.instance.readNextClipboardDataChange in the
   * service worker.
   */
  private onClipboardDataChanged_(forceRead: boolean): ClipboardData {
    if (!forceRead && !this.lastClipboardEvent_) {
      return {};
    }

    const eventType = this.lastClipboardEvent_;
    this.lastClipboardEvent_ = undefined;

    const textarea = document.createElement('textarea');
    document.body.appendChild(textarea);
    textarea.focus();
    document.execCommand('paste');
    const clipboardContent = textarea.value;
    textarea.remove();

    return {eventType, clipboardContent};
  }
}

class OffscreenSpeechSynthesis {
  static instance?: OffscreenSpeechSynthesis;

  constructor() {
    if (window.speechSynthesis) {
      window.speechSynthesis.onvoiceschanged = () => {
        if (OffscreenChromeVoxState.isReady()) {
          BackgroundBridge.LocaleOutputHelper.onVoicesChanged();
          BackgroundBridge.PrimaryTts.onVoicesChanged();
        }
      };
    }

    BridgeHelper.registerHandler(
        TARGET, Action.SHOULD_SET_DEFAULT_VOICE,
        () => this.shouldSetDefaultVoice_());
  }

  static init(): void {
    if (OffscreenSpeechSynthesis.instance) {
      throw new Error(
          'Error: trying to create two instances of singleton ' +
          'OffscreenSpeechSynthesis.');
    }
    OffscreenSpeechSynthesis.instance = new OffscreenSpeechSynthesis();
  }

  // If the SpeechSynthesis API is not available it indicates we are
  // in chromecast and the default voice must be set.
  private shouldSetDefaultVoice_(): boolean {
    if (!window.speechSynthesis) {
      return true;
    }
    return false;
  }
}

class OffscreenBrailleDisplayManager {
  static instance?: OffscreenBrailleDisplayManager;

  constructor() {
    BridgeHelper.registerHandler(
        TARGET, Action.IMAGE_DATA_FROM_URL,
        (imageDataUrl: string, imageState: StateWithMaxCellHeight) =>
            this.getImageDataFromUrl_(imageDataUrl, imageState));
  }

  static init(): void {
    if (OffscreenBrailleDisplayManager.instance) {
      throw new Error(
          'Error: trying to create two instances of singleton ' +
          'OffscreenBrailleDisplayManager.');
    }
    OffscreenBrailleDisplayManager.instance =
        new OffscreenBrailleDisplayManager();
  }

  getImageDataFromUrl_(
      imageDataUrl: string, imageState: StateWithMaxCellHeight): Promise<string> {
    const {rows, columns, cellWidth, cellHeight} = imageState;
    const imgElement = document.createElement('img');
    imgElement.src = imageDataUrl;
    return new Promise(resolve => {
      imgElement.onload = () => {
        const canvas = document.createElement('canvas');
        const context = canvas.getContext('2d')!;
        canvas.width = columns * cellWidth;
        canvas.height = rows * cellHeight;
        context.drawImage(imgElement, 0, 0, canvas.width, canvas.height);
        const imageData: Uint8ClampedArray =
          context.getImageData(0, 0, canvas.width, canvas.height).data;

        // Serialize the Uint8ClampedArray in order to send via chrome's message
        // passing API.
        let binary = '';
        for (let i = 0; i < imageData.length; i++) {
          binary += String.fromCharCode(imageData[i]);
        }
        resolve(window.btoa(binary));
      };
    });
  }
}

class OffscreenMathHandler {
  static instance?: OffscreenMathHandler;

  constructor() {
    BridgeHelper.registerHandler(
        TARGET, Action.SRE_MOVE, (keyCode: number) => this.sreMove(keyCode));

    BridgeHelper.registerHandler(
        TARGET, Action.SRE_WALK, (mathml: string) => this.sreWalk(mathml));
  }

  static init(): void {
    if (OffscreenMathHandler.instance) {
      throw new Error(
          'Error: trying to create two instances of singleton ' +
          'OffscreenMathHandler.');
    }
    OffscreenMathHandler.instance = new OffscreenMathHandler();
  }

  sreMove(keyCode: number): string {
    return SRE.move(keyCode);
  }

  sreWalk(mathml: string): string {
    return SRE.walk(mathml);
  }
}


// OffscreenChromeVoxState.init() must be called before ChromeVox
// finishes initialization.
OffscreenChromeVoxState.init();

OffscreenLearnModeKeyboardHandler.init();
OffscreenClipboardHandler.init();
OffscreenSpeechSynthesis.init();
OffscreenBrailleDisplayManager.init();
OffscreenMathHandler.init();
EarconEngine.init();
LibLouisWorker.init();
