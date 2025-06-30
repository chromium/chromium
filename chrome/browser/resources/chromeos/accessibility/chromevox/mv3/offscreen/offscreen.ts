// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SRE} from '/chromevox/mv3/third_party/sre/sre_browser.js';

import {EarconEngine} from '../background/earcon_engine.js';
import {BackgroundBridge} from '../common/background_bridge.js';
import {InternalKeyEvent} from '../common/internal_key_event.js';
import {LearnModeBridge} from '../common/learn_mode_bridge.js';
import {OffscreenCommandType} from '../common/offscreen_command_type.js';

import {LibLouisWorker} from './liblouis_worker.js';

type SendResponse = (value: any) => void;

/**
 * Receives messages and routes them to the proper class for handling.
 */
class OffscreenMessageHandler {
  static instance?: OffscreenMessageHandler;

  constructor() {
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: chrome.runtime.MessageSender,
         sendResponse: SendResponse) =>
            this.handleMessage_(message, sendResponse));
  }

  static init(): void {
    if (OffscreenMessageHandler.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenMessageHandler.';
    }

    OffscreenMessageHandler.instance = new OffscreenMessageHandler();
  }


  /**
   * Handles messages from various contexts (e.g. service worker, learn mode,
   * etc.). Returns true if the `sendResponse` callback should be kept alive,
   * false otherwise.
   */
  private handleMessage_(message: any|undefined, sendResponse: SendResponse):
      boolean {
    switch (message.command) {
      case OffscreenCommandType.IMAGE_DATA_FROM_URL:
        OffscreenBrailleDisplayManager.instance!.getImageDataFromUrl(
            message, sendResponse);
        // The response is asynchronous and the callback must be kept alive.
        return true;
      case OffscreenCommandType.LEARN_MODE_REGISTER_LISTENERS:
        OffscreenLearnModeKeyboardHandler.instance!.registerListeners();
        break;
      case OffscreenCommandType.LEARN_MODE_REMOVE_LISTENERS:
        OffscreenLearnModeKeyboardHandler.instance!.removeListeners();
        break;
      case OffscreenCommandType.SRE_MOVE:
        OffscreenMathHandler.instance!.sreMove(sendResponse, message.keyCode);
        break;
      case OffscreenCommandType.SRE_WALK:
        OffscreenMathHandler.instance!.sreWalk(sendResponse, message.mathml);
        break;
      case OffscreenCommandType.ON_CLIPBOARD_DATA_CHANGED:
        const forceRead = message.forceRead as boolean;
        OffscreenClipboardHandler.instance!.onClipboardDataChanged(
            sendResponse, forceRead);
        break;
      case OffscreenCommandType.SHOULD_SET_DEFAULT_VOICE:
        OffscreenSpeechSynthesis.instance!.shouldSetDefaultVoice(sendResponse);
        break;
    }

    return false;
  }
}

/**
 * Handles keydown and keyup events on the document and sends serialized key
 * event data to be processed in the service worker's BackgroundKeyboardHandler.
 */
class OffscreenBackgroundKeyboardHandler {
  static instance?: OffscreenBackgroundKeyboardHandler;

  constructor() {
    document.addEventListener(
        'keydown', (event) => this.onKeyDown_(event), false);
    document.addEventListener('keyup', (event) => this.onKeyUp_(event), false);
  }

  static init(): void {
    if (OffscreenBackgroundKeyboardHandler.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'BackgroundKeyboardHandler.';
    }
    OffscreenBackgroundKeyboardHandler.instance =
        new OffscreenBackgroundKeyboardHandler();
  }

  /**
   * Handles key down events using the offscreen DOM and forwards them to the
   * ChromeVox service worker.
   */
  private async onKeyDown_(evt: KeyboardEvent): Promise<void> {
    const internalEvt = new InternalKeyEvent(evt);
    const stopPropagation =
        await BackgroundBridge.BackgroundKeyboardHandler.onKeyDown(internalEvt);
    if (stopPropagation) {
      evt.preventDefault();
      evt.stopPropagation();
    }
  }

  /**
   * Handles key up events using the offscreen DOM and forwards them to the
   * ChromeVox service worker.
   */
  private async onKeyUp_(evt: KeyboardEvent): Promise<void> {
    const internalEvt = new InternalKeyEvent(evt);
    const stopPropagation =
        await BackgroundBridge.BackgroundKeyboardHandler.onKeyUp(internalEvt);
    if (stopPropagation) {
      evt.preventDefault();
      evt.stopPropagation();
    }
  }
}

/**
 * Handles keydown and keyup events when Learn Mode is initiated.
 */
class OffscreenLearnModeKeyboardHandler {
  static instance?: OffscreenLearnModeKeyboardHandler;

  static init(): void {
    if (OffscreenLearnModeKeyboardHandler.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenLearnModeKeyboardHandler.';
    }
    OffscreenLearnModeKeyboardHandler.instance =
        new OffscreenLearnModeKeyboardHandler();
  }

  registerListeners(): void {
    window.addEventListener('keydown', this.onKeyDown_, true);
    window.addEventListener('keyup', this.onKeyUp_, true);
    window.addEventListener('keypress', this.onKeyPress_, true);
  }

  removeListeners(): void {
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
  }

  static init(): void {
    if (OffscreenClipboardHandler.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenClipboardHandler.';
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
  onClipboardDataChanged(sendResponse: SendResponse, forceRead: boolean): void {
    if (!forceRead && !this.lastClipboardEvent_) {
      return;
    }

    const eventType = this.lastClipboardEvent_;
    this.lastClipboardEvent_ = undefined;

    const textarea = document.createElement('textarea');
    document.body.appendChild(textarea);
    textarea.focus();
    document.execCommand('paste');
    const clipboardContent = textarea.value;
    textarea.remove();

    sendResponse({eventType, clipboardContent});
  }
}

class OffscreenSpeechSynthesis {
  static instance?: OffscreenSpeechSynthesis;

  constructor() {
    if (window.speechSynthesis) {
      window.speechSynthesis.onvoiceschanged = () => {
        BackgroundBridge.LocaleOutputHelper.onVoicesChanged();
        BackgroundBridge.PrimaryTts.onVoicesChanged();
      };
    }
  }

  static init(): void {
    if (OffscreenSpeechSynthesis.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenSpeechSynthesis.';
    }
    OffscreenSpeechSynthesis.instance = new OffscreenSpeechSynthesis();
  }

  // If the SpeechSynthesis API is not available it indicates we are
  // in chromecast and the default voice must be set.
  shouldSetDefaultVoice(sendResponse: SendResponse): void {
    if (!window.speechSynthesis) {
      sendResponse(true);
      return;
    }
    sendResponse(false);
  }
}

class OffscreenBrailleDisplayManager {
  static instance?: OffscreenBrailleDisplayManager;

  static init(): void {
    if (OffscreenBrailleDisplayManager.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenBrailleDisplayManager.';
    }
    OffscreenBrailleDisplayManager.instance =
        new OffscreenBrailleDisplayManager();
  }

  getImageDataFromUrl(message: any, sendResponse: SendResponse): void {
    const {imageDataUrl, imageState: {rows, columns, cellWidth, cellHeight}} =
        message;

    const imgElement = document.createElement('img');
    imgElement.src = imageDataUrl;
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
      sendResponse({data: window.btoa(binary), length: imageData.length});
    }
  }
}

class OffscreenMathHandler {
  static instance?: OffscreenMathHandler;

  static init(): void {
    if (OffscreenMathHandler.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenMathHandler.';
    }
    OffscreenMathHandler.instance = new OffscreenMathHandler();
  }

  sreMove(sendResponse: SendResponse, keyCode: number): void {
    sendResponse(SRE.move(keyCode));
  }

  sreWalk(sendResponse: SendResponse, mathml: string): void {
    sendResponse(SRE.walk(mathml));
  }
}

OffscreenBackgroundKeyboardHandler.init();
OffscreenLearnModeKeyboardHandler.init();
OffscreenClipboardHandler.init();
OffscreenSpeechSynthesis.init();
OffscreenBrailleDisplayManager.init();
OffscreenMathHandler.init();
EarconEngine.init();
LibLouisWorker.init();

OffscreenMessageHandler.init();
