// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EarconEngine} from '../background/earcon_engine.js';
import {BackgroundBridge} from '../common/background_bridge.js';
import {InternalKeyEvent} from '../common/internal_key_event.js';
import {LearnModeBridge} from '../common/learn_mode_bridge.js';
import {OffscreenCommandType} from '../common/offscreen_command_type.js';

import {LibLouisWorker} from './liblouis_worker.js';

type MessageSender = chrome.runtime.MessageSender;
type SendResponse = (value: any) => void;

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

  constructor() {
    // Add listeners to chrome.runtime
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: chrome.runtime.MessageSender,
         _sendResponse: SendResponse) =>
            this.handleMessageFromLearnMode_(message));
  }

  private handleMessageFromLearnMode_(message: any|undefined): boolean {
    switch (message.command) {
      case OffscreenCommandType.LEARN_MODE_REGISTER_LISTENERS:
        this.registerListeners_();
        break;
      case OffscreenCommandType.LEARN_MODE_REMOVE_LISTENERS:
        this.removeListeners_();
        break;
    }
    // Returns false as the response is not asynchronous and the callback does
    // not need to be kept alive.
    return false;
  }

  static init(): void {
    if (OffscreenLearnModeKeyboardHandler.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenLearnModeKeyboardHandler.';
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

    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: MessageSender,
         sendResponse: SendResponse) =>
            this.handleMessageFromServiceWorker_(message, sendResponse));
  }

  private handleMessageFromServiceWorker_(
      message: any|undefined, sendResponse: SendResponse): boolean {
    switch (message['command']) {
      case OffscreenCommandType.ON_CLIPBOARD_DATA_CHANGED:
        const forceRead = message['forceRead'] as boolean;
        this.onClipboardDataChanged_(sendResponse, forceRead);
        break;
    }
    // Returns false as the response is not asynchronous and the callback does
    // not need to be kept alive.
    return false;
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
  private onClipboardDataChanged_(
      sendResponse: SendResponse, forceRead: boolean): void {
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

    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: MessageSender,
         sendResponse: SendResponse) =>
            this.handleMessageFromServiceWorker_(message, sendResponse));
  }

  private handleMessageFromServiceWorker_(
      message: any|undefined, sendResponse: SendResponse): boolean {
    switch (message['command']) {
      case OffscreenCommandType.SHOULD_SET_DEFAULT_VOICE:
        this.shouldSetDefaultVoice_(sendResponse);
        break;
    }
    // Returns false as the response is not asynchronous and the callback does
    // not need to be kept alive.
    return false;
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
  private shouldSetDefaultVoice_(sendResponse: SendResponse): void {
    if (!window.speechSynthesis) {
      sendResponse(true);
      return;
    }
    sendResponse(false);
  }
}

class OffscreenBrailleDisplayManager {
  static instance?: OffscreenBrailleDisplayManager;

  constructor() {
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: MessageSender,
         sendResponse: SendResponse) =>
            this.handleMessageFromServiceWorker_(message, sendResponse));
  }

  private handleMessageFromServiceWorker_(
      message: any|undefined, sendResponse: SendResponse): boolean {
    switch (message['command']) {
      case OffscreenCommandType.IMAGE_DATA_FROM_URL:
        this.getImageDataFromUrl_(message, sendResponse);
        // Returns true as the response is asynchronous and the callback
        // must be kept alive.
        return true;
    }
    return false;
  }

  static init(): void {
    if (OffscreenBrailleDisplayManager.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenBrailleDisplayManager.';
    }
    OffscreenBrailleDisplayManager.instance =
        new OffscreenBrailleDisplayManager();
  }

  getImageDataFromUrl_(message: any, sendResponse: SendResponse): void {
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


OffscreenBackgroundKeyboardHandler.init();
OffscreenLearnModeKeyboardHandler.init();
OffscreenClipboardHandler.init();
OffscreenSpeechSynthesis.init();
OffscreenBrailleDisplayManager.init();
EarconEngine.init();
LibLouisWorker.init();
