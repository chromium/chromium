// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for the background page.
 */
cca.bg = {};

/**
 * Fixed minimum width of the window inner-bounds in pixels.
 * @type {number}
 * @const
 */
cca.bg.MIN_WIDTH = 768;

/**
 * Initial apsect ratio of the window inner-bounds.
 * @type {number}
 * @const
 */
cca.bg.INITIAL_ASPECT_RATIO = 1.7777777777;

/**
 * Top bar color of the window.
 * @type {string}
 * @const
 */
cca.bg.TOPBAR_COLOR = '#000000';

/**
 * It's used in test to ensure that we won't connect to the main.html target
 * before the window is created, otherwise the window might disappear.
 * @type {?function(string): undefined}
 */
cca.bg.onAppWindowCreatedForTesting = null;

/**
 * Background object for handling launch event.
 * @type {?cca.bg.Background}
 */
cca.bg.background = null;

/**
 * State of cca.bg.Window.
 * @enum {string}
 */
cca.bg.WindowState = {
  UNINIT: 'uninitialized',
  LAUNCHING: 'launching',
  ACTIVE: 'active',
  SUSPENDING: 'suspending',
  SUSPENDED: 'suspended',
  RESUMING: 'resuming',
  CLOSING: 'closing',
  CLOSED: 'closed',
};

/* eslint-disable no-unused-vars */

/**
 * @typedef {{
 *   suspend: !function(),
 *   resume: !function(),
 * }}
 */
cca.bg.WindowOperations;

/* eslint-enable no-unused-vars */

/**
 * Wrapper of AppWindow for tracking its state.
 */
cca.bg.Window = class {
  /**
   * @param {!function(cca.bg.Window)} onActive Called when window become active
   *     state.
   * @param {!function(cca.bg.Window)} onSuspended Called when window become
   *     suspended state.
   * @param {!function(cca.bg.Window)} onClosed Called when window become closed
   *     state.
   * @param {cca.intent.Intent=} intent Intent to be handled by the app window.
   *     Set to null for app window not launching from intent.
   */
  constructor(onActive, onSuspended, onClosed, intent = null) {
    /**
     * @type {!function(!cca.bg.Window)}
     * @private
     */
    this.onActive_ = onActive;

    /**
     * @type {!function(!cca.bg.Window)}
     * @private
     */
    this.onSuspended_ = onSuspended;

    /**
     * @type {!function(!cca.bg.Window)}
     * @private
     */
    this.onClosed_ = onClosed;

    /**
     * @type {?cca.intent.Intent}
     * @private
     */
    this.intent_ = intent;

    /**
     * @type {?chrome.app.window.AppWindow}
     * @private
     */
    this.appWindow_ = null;

    /**
     * @type {!cca.bg.WindowState}
     * @private
     */
    this.state_ = cca.bg.WindowState.UNINIT;
  }

  /**
   * Gets state of the window.
   * @return {cca.bg.WindowState}
   */
  get state() {
    return this.state_;
  }

  /**
   * Creates app window and launches app.
   */
  launch() {
    this.state_ = cca.bg.WindowState.LAUNCHING;

    // The height will be later calculated to match video aspect ratio once the
    // stream is available.
    var initialHeight =
        Math.round(cca.bg.MIN_WIDTH / cca.bg.INITIAL_ASPECT_RATIO);

    const windowId =
        this.intent_ !== null ? `main-${this.intent_.intentId}` : 'main';
    const windowUrl = 'views/main.html' +
        (this.intent_ !== null ? this.intent_.url.search : '');

    chrome.app.window.create(
        windowUrl, {
          id: windowId,
          frame: {color: cca.bg.TOPBAR_COLOR},
          hidden: true,  // Will be shown from main.js once loaded.
          innerBounds: {
            width: cca.bg.MIN_WIDTH,
            height: initialHeight,
            minWidth: cca.bg.MIN_WIDTH,
            left: Math.round((window.screen.availWidth - cca.bg.MIN_WIDTH) / 2),
            top: Math.round((window.screen.availHeight - initialHeight) / 2),
          },
        },
        (appWindow) => {
          this.appWindow_ = appWindow;
          this.appWindow_.onClosed.addListener(() => {
            chrome.storage.local.set({maximized: appWindow.isMaximized()});
            chrome.storage.local.set({fullscreen: appWindow.isFullscreen()});
            this.state_ = cca.bg.WindowState.CLOSED;
            if (this.intent_ !== null && !this.intent_.done) {
              this.intent_.cancel();
            }
            this.onClosed_(this);
          });
          const wnd = appWindow.contentWindow;
          wnd.intent = this.intent_;
          wnd.onActive = () => {
            this.state_ = cca.bg.WindowState.ACTIVE;
            // For intent only requiring open camera with specific mode without
            // returning the capture result, called onIntentHandled() right
            // after app successfully launched.
            if (this.intent_ !== null && !this.intent_.shouldHandleResult) {
              this.intent_.finish();
            }
            this.onActive_(this);
          };
          wnd.onSuspended = () => {
            this.state_ = cca.bg.WindowState.SUSPENDED;
            this.onSuspended_(this);
          };
          if (cca.bg.onAppWindowCreatedForTesting !== null) {
            cca.bg.onAppWindowCreatedForTesting(windowUrl);
          }
        });
  }

  /**
   * Gets WindowOperations associated with this window.
   * @return {!cca.bg.WindowOperations}
   * @throws {Error} Throws when no WindowOperations is associated with the
   *     window.
   * @private
   */
  getWindowOps_() {
    const ops =
        /** @type {(!cca.bg.WindowOperations|undefined)} */ (
            this.appWindow_.contentWindow['ops']);
    if (ops === undefined) {
      throw new Error('WindowOperations not found on target window.');
    }
    return ops;
  }

  /**
   * Suspends the app window.
   */
  suspend() {
    if (this.state_ === cca.bg.WindowState.LAUNCHING) {
      console.error('Call suspend() while window is still launching.');
      return;
    }
    this.state_ = cca.bg.WindowState.SUSPENDING;
    this.getWindowOps_().suspend();
  }

  /**
   * Resumes the app window.
   */
  resume() {
    this.state_ = cca.bg.WindowState.RESUMING;
    this.getWindowOps_().resume();
  }

  /**
   * Closes the app window.
   */
  close() {
    this.state_ = cca.bg.WindowState.CLOSING;
    this.appWindow_.close();
  }
};

/**
 * Launch event handler runs in background.
 */
cca.bg.Background = class {
  /**
   */
  constructor() {
    /**
     * Launch window handles launch event triggered from app launcher.
     * @type {?cca.bg.Window}
     * @private
     */
    this.launcherWindow_ = null;

    /**
     * Intent window handles launch event triggered from ARC++ intent.
     * @type {?cca.bg.Window}
     * @private
     */
    this.intentWindow_ = null;

    /**
     * The pending intent arrived when foreground window is busy.
     * @type {?cca.intent.Intent}
     */
    this.pendingIntent_ = null;
  }

  /**
   * Checks and logs any violation of background transition logic.
   * @param {boolean} assertion Condition to be asserted.
   * @param {string|function(): string} message Logged message.
   * @private
   */
  assert_(assertion, message) {
    if (!assertion) {
      console.error(typeof message === 'string' ? message : message());
    }
    // TODO(inker): Cleans up states and starts over after any violation.
  }

  /**
   * Processes the pending intent.
   * @private
   */
  processPendingIntent_() {
    if (!this.pendingIntent_) {
      console.error('Call processPendingIntent_() without intent present.');
      return;
    }
    this.intentWindow_ = this.createIntentWindow_(this.pendingIntent_);
    this.pendingIntent_ = null;
    this.intentWindow_.launch();
  }

  /**
   * Returns a Window object handling launch event triggered from app launcher.
   * @return {!cca.bg.Window}
   * @private
   */
  createLauncherWindow_() {
    const onActive = (wnd) => {
      this.assert_(wnd === this.launcherWindow_, 'Wrong active launch window.');
      this.assert_(
          !this.intentWindow_,
          'Launch window is active while handling intent window.');
      if (this.pendingIntent_ !== null) {
        wnd.suspend();
      }
    };
    const onSuspended = (wnd) => {
      this.assert_(
          wnd === this.launcherWindow_, 'Wrong suspended launch window.');
      this.assert_(
          !this.intentWindow_,
          'Launch window is suspended while handling intent window.');
      if (this.pendingIntent_ === null) {
        this.assert_(
            false, 'Launch window is not suspended by some pending intent');
        wnd.resume();
        return;
      }
      this.processPendingIntent_();
    };
    const onClosed = (wnd) => {
      this.assert_(wnd === this.launcherWindow_, 'Wrong closed launch window.');
      this.launcherWindow_ = null;
      if (this.pendingIntent_ !== null) {
        this.processPendingIntent_();
      }
    };
    return new cca.bg.Window(onActive, onSuspended, onClosed);
  }

  /**
   * Returns a Window object handling launch event triggered from ARC++ intent.
   * @param {!cca.intent.Intent} intent Intent forwarding from ARC++.
   * @return {!cca.bg.Window}
   * @private
   */
  createIntentWindow_(intent) {
    const onActive = (wnd) => {
      this.assert_(wnd === this.intentWindow_, 'Wrong active intent window.');
      this.assert_(
          !this.launcherWindow_ ||
              this.launcherWindow_.state === cca.bg.WindowState.SUSPENDED,
          () => `Launch window is ${
              this.launcherWindow_.state} when intent window is active.`);
      if (this.pendingIntent_) {
        wnd.close();
      }
    };
    const onSuspended = (wnd) => {
      this.assert_(
          wnd === this.intentWindow_, 'Wrong suspended intent window.');
      this.assert_(false, 'Intent window should not be suspended.');
    };
    const onClosed = (wnd) => {
      this.assert_(wnd === this.intentWindow_, 'Wrong closed intent window.');
      this.assert_(
          !this.launcherWindow_ ||
              this.launcherWindow_.state === cca.bg.WindowState.SUSPENDED,
          () => `Launch window is ${
              this.launcherWindow_.state} when intent window is closed.`);
      this.intentWindow_ = null;
      if (this.pendingIntent_) {
        this.processPendingIntent_();
      } else if (this.launcherWindow_) {
        this.launcherWindow_.resume();
      }
    };
    return new cca.bg.Window(onActive, onSuspended, onClosed, intent);
  }

  /**
   * Handles launch event triggered from app launcher.
   */
  launchApp() {
    if (this.launcherWindow_ || this.intentWindow_) {
      return;
    }
    this.assert_(
        !this.pendingIntent_,
        'Pending intent is not processed when launch new window.');
    this.launcherWindow_ = this.createLauncherWindow_();
    this.launcherWindow_.launch();
  }

  /**
   * Closes the existing pending intent and replaces it with a new incoming
   * intent.
   * @param {!cca.intent.Intent} intent New incoming intent.
   * @private
   */
  replacePendingIntent_(intent) {
    if (this.pendingIntent_) {
      this.pendingIntent_.cancel();
    }
    this.pendingIntent_ = intent;
  }

  /**
   * Handles launch event triggered from ARC++ intent.
   * @param {!cca.intent.Intent} intent Intent forwarding from ARC++.
   */
  launchIntent(intent) {
    if (this.intentWindow_) {
      switch (this.intentWindow_.state) {
        case cca.bg.WindowState.LAUNCHING:
        case cca.bg.WindowState.CLOSING:
          this.replacePendingIntent_(intent);
          break;
        case cca.bg.WindowState.ACTIVE:
          this.replacePendingIntent_(intent);
          this.intentWindow_.close();
          break;
        default:
          this.assert_(
              false,
              `Intent window is ${
                  this.intentWindow_.state} when launch new intent window.`);
      }
    } else if (this.launcherWindow_) {
      switch (this.launcherWindow_.state) {
        case cca.bg.WindowState.LAUNCHING:
        case cca.bg.WindowState.SUSPENDING:
        case cca.bg.WindowState.RESUMING:
        case cca.bg.WindowState.CLOSING:
          this.replacePendingIntent_(intent);
          break;
        case cca.bg.WindowState.ACTIVE:
          this.assert_(
              !this.pendingIntent_,
              'Pending intent is not processed when launch window is active.');
          this.replacePendingIntent_(intent);
          this.launcherWindow_.suspend();
          break;
        default:
          this.assert_(
              false,
              `Launch window is ${
                  this.launcherWindow_.state} when launch new intent window.`);
      }
    } else {
      this.intentWindow_ = this.createIntentWindow_(intent);
      this.intentWindow_.launch();
    }
  }
};

/**
 * Handles messages from the test extension used in Tast.
 * @param {*} message The message sent by the calling script.
 * @param {!MessageSender} sender
 * @param {function(string): undefined} sendResponse The callback function which
 *     expects to receive the url of the window when the window is successfully
 *     created.
 * @return {boolean|undefined} True to indicate the response is sent
 *     asynchronously.
 */
cca.bg.handleExternalMessageFromTest = function(message, sender, sendResponse) {
  if (sender.id !== 'behllobkkfkfnphdnhnkndlbkcpglgmj') {
    console.warn(`Unknown sender id: ${sender.id}`);
    return;
  }
  switch (message.action) {
    case 'SET_WINDOW_CREATED_CALLBACK':
      cca.bg.onAppWindowCreatedForTesting = sendResponse;
      return true;
    default:
      console.warn(`Unknown action: ${message.action}`);
  }
};

chrome.app.runtime.onLaunched.addListener((launchData) => {
  if (!cca.bg.background) {
    cca.bg.background = new cca.bg.Background();
  }
  try {
    if (launchData.url) {
      const intent = cca.intent.Intent.create(new URL(launchData.url));
      cca.bg.background.launchIntent(intent);
    } else {
      cca.bg.background.launchApp();
    }
  } catch (e) {
    console.error(e.stack);
  }
});

chrome.runtime.onMessageExternal.addListener(
    cca.bg.handleExternalMessageFromTest);
