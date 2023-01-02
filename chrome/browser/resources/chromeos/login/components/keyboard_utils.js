// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for setting the focus direction.
 * @enum {string}
 */
const FocusDirection = {
  Forward: 'forwardFocus',
  Backward: 'backwardFocus',
};

export class KeyboardUtils {
  /**
   * Initializes event handling for arrow keys driven focus flow.
   */
  initializeKeyboardFlow() {
    document.addEventListener('keydown', this.onKeyDown_.bind(this), true);
    document.addEventListener('keypress', this.onKeyIgnore_.bind(this), true);
    document.addEventListener('keyup', this.onKeyIgnore_.bind(this), true);
  }

  /**
   * This method is called by the Enterprise Enrollment screen when it is
   * about to be shown. That screen injects a modified version of this class
   * `InjectedKeyboardUtils` into the WebView that hosts the Gaia page.
   */
  enableHandlingOfInjectedKeyboardUtilsMessages() {
    window.addEventListener(
        'message', this.handleMessageFromInjectedKeyboardUtils.bind(this));
  }

  handleMessageFromInjectedKeyboardUtils(event) {
    const focusDir = event.data;
    if (focusDir == FocusDirection.Forward ||
        focusDir == FocusDirection.Backward) {
      this.onAdvanceFocus(focusDir);
    }
  }

  /**
   * Handles the actual focus advancing by raising tab/shift-tab key events
   * on C++ side.
   * @param {FocusDirection} focusDir The direction to change focus to.
   */
  onAdvanceFocus(focusDir) {
    const reverse = focusDir === FocusDirection.Backward;
    chrome.send('raiseTabKeyEvent', [reverse]);
  }

  /**
   * Swallows keypress and keyup events of arrow keys.
   * @param {!Event} event Raised event.
   * @private
   */
  onKeyIgnore_(event) {
    event = /** @type {!KeyboardEvent} */ (event);

    if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey) {
      return;
    }

    if (event.key == 'ArrowLeft' || event.key == 'ArrowRight' ||
        event.key == 'ArrowUp' || event.key == 'ArrowDown') {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  /**
   * Handles arrow key events.
   * @param {!Event} event Raised event.
   * @private
   */
  onKeyDown_(event) {
    event = /** @type {!KeyboardEvent} */ (event);

    if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey) {
      return;
    }

    // The networks list in the Chrome OOBE has an iron-list which uses arrow
    // keys to navigate elements. Tab events will remove focus from the list.
    // See crbug.com/1083145
    if (document.activeElement ===
            document.getElementById('network-selection') &&
        document.activeElement.shadowRoot.activeElement.tagName ==
            'NETWORK-SELECT-LOGIN' &&
        (event.key == 'ArrowUp' || event.key == 'ArrowDown')) {
      return;
    }

    const arrowBackwards = event.key == 'ArrowLeft' || event.key == 'ArrowUp';
    const arrowForwards = event.key == 'ArrowRight' || event.key == 'ArrowDown';
    if (arrowBackwards || arrowForwards) {
      // Event is being handled here.
      event.stopPropagation();

      // Do not map arrow key events to tab events if the user is currently
      // focusing an input element and presses on the left or right arrows.
      if (document.activeElement.tagName == 'INPUT' &&
          (event.key == 'ArrowLeft' || event.key == 'ArrowRight')) {
        // Default event handling will occur.
        return;
      }

      this.onAdvanceFocus(
          arrowBackwards ? FocusDirection.Backward : FocusDirection.Forward);
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
  /**
   * Initial Message that should be sent by the WebView in which
   * InjectedKeyboardUtils lives in order to start communicating.
   */
  static get INITIAL_MSG() {
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
   * @override
   */
  initializeKeyboardFlow() {
    super.initializeKeyboardFlow();
    window.addEventListener('message', this.onInitMessage_.bind(this));
  }

  /**
   * Send a message to OOBE to advance the focus forwards, or backwards.
   * @param {FocusDirection} focusDir The direction to change focus to.
   * @override
   */
  onAdvanceFocus(focusDir) {
    if (this.hostWindow) {
      this.hostWindow.postMessage(focusDir, this.hostOrigin);
    }
  }

  /**
   * Handles the initial messaging posted from webview, where this script is
   * injected.
   * @param {Event} event Message event posted from webview.
   * @private
   */
  onInitMessage_(event) {
    if (event.data == InjectedKeyboardUtils.INITIAL_MSG &&
        event.origin == 'chrome://oobe') {
      this.hostWindow = event.source;
      this.hostOrigin = event.origin;
    }
  }
}
