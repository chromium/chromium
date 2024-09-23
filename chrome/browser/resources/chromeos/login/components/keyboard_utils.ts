// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for setting the focus direction.
 */
enum FocusDirection {
  FORWARD = 'forwardFocus',
  BACKWARD = 'backwardFocus',
}

export class KeyboardUtils {
  /**
   * Initializes event handling for arrow keys driven focus flow.
   */
  initializeKeyboardFlow(): void {
    document.addEventListener('keydown', this.onKeyDown.bind(this), true);
    document.addEventListener('keypress', this.onKeyIgnore.bind(this), true);
    document.addEventListener('keyup', this.onKeyIgnore.bind(this), true);
  }

  /**
   * This method is called by the Enterprise Enrollment screen when it is
   * about to be shown. That screen injects a modified version of this class
   * `InjectedKeyboardUtils` into the WebView that hosts the Gaia page.
   */
  enableHandlingOfInjectedKeyboardUtilsMessages(): void {
    window.addEventListener(
        'message', this.handleMessageFromInjectedKeyboardUtils.bind(this));
  }

  private handleMessageFromInjectedKeyboardUtils(event: MessageEvent) {
    const focusDir = event.data;
    if (focusDir === FocusDirection.FORWARD ||
        focusDir === FocusDirection.BACKWARD) {
      this.onAdvanceFocus(focusDir);
    }
  }

  /**
   * Handles the actual focus advancing by raising tab/shift-tab key events
   * on C++ side.
   * @param focusDir The direction to change focus to.
   */
  onAdvanceFocus(focusDir: FocusDirection): void {
    const reverse = focusDir === FocusDirection.BACKWARD;
    chrome.send('raiseTabKeyEvent', [reverse]);
  }

  /**
   * Swallows keypress and keyup events of arrow keys.
   * @param event Raised event.
   */
  private onKeyIgnore(event: KeyboardEvent): void {
    if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey) {
      return;
    }

    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight' ||
        event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  /**
   * Handles arrow key events.
   * @param event Raised event.
   */
  private onKeyDown(event: KeyboardEvent): void {
    if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey) {
      return;
    }

    // The networks list in the Chrome OOBE has an iron-list which uses arrow
    // keys to navigate elements. Tab events will remove focus from the list.
    // See crbug.com/1083145
    if (document.activeElement ===
            document.getElementById('network-selection') &&
        document.activeElement?.shadowRoot?.activeElement?.tagName ===
            'NETWORK-SELECT-LOGIN' &&
        (event.key === 'ArrowUp' || event.key === 'ArrowDown')) {
      return;
    }

    const arrowBackwards = event.key === 'ArrowLeft' || event.key === 'ArrowUp';
    const arrowForwards = event.key === 'ArrowRight' || event.key === 'ArrowDown';
    if (arrowBackwards || arrowForwards) {
      // Event is being handled here.
      event.stopPropagation();

      // Do not map arrow key events to tab events if the user is currently
      // focusing an input element and presses on the left or right arrows.
      if (document.activeElement?.tagName === 'INPUT' &&
          (event.key === 'ArrowLeft' || event.key === 'ArrowRight')) {
        // Default event handling will occur.
        return;
      }

      this.onAdvanceFocus(
          arrowBackwards ? FocusDirection.BACKWARD : FocusDirection.FORWARD);
      event.preventDefault();
    }
  }
}

/**
 * KeyboardUtils to be injected into the enrollment flow. Instead of handling
 * the focus directly like in KeyboardUtils::onAdvanceFocus, this class
 * overrides the 'onAdvanceFocus' method to send a message to OOBE instead.
 */
export class InjectedKeyboardUtils extends KeyboardUtils {
  private hostWindow: chrome.webviewTag.ContentWindow|null;
  private hostOrigin: string|null;

  /**
   * Initial Message that should be sent by the WebView in which
   * InjectedKeyboardUtils lives in order to start communicating.
   */
  static get INITIAL_MSG(): string {
    return 'initialMessage';
  }

  constructor() {
    super();
    this.hostWindow = null;
    this.hostOrigin = null;
  }

  /**
   * Initializes event handling for arrow keys driven focus flow in the base
   * class and listens for 'message' events that come from OOBE.
   */
  override initializeKeyboardFlow(): void {
    super.initializeKeyboardFlow();
    window.addEventListener('message', this.onInitMessage.bind(this));
  }

  /**
   * Send a message to OOBE to advance the focus forwards, or backwards.
   * @param focusDir The direction to change focus to.
   */
  override onAdvanceFocus(focusDir: FocusDirection): void {
    if (this.hostWindow && this.hostOrigin) {
      this.hostWindow.postMessage(focusDir, this.hostOrigin);
    }
  }

  /**
   * Handles the initial messaging posted from webview, where this script is
   * injected.
   * @param event Message event posted from webview.
   */
  private onInitMessage(event: MessageEvent): void {
    if (event.data === InjectedKeyboardUtils.INITIAL_MSG &&
        event.origin === 'chrome://oobe') {
      this.hostWindow = event.source as chrome.webviewTag.ContentWindow;
      this.hostOrigin = event.origin;
    }
  }
}
