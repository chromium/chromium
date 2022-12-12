// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeEventHandler} from '../../common/chrome_event_handler.js';
import {EventHandler} from '../../common/event_handler.js';
import {RectUtil} from '../../common/rect_util.js';

const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;

/**
 * Main class for the Chrome OS magnifier.
 */
export class Magnifier {
  /**
   * @param {!Magnifier.Type} type The type of magnifier in use.
   */
  constructor(type) {
    /** @const {!Magnifier.Type} */
    this.type = type;

    /**
     * Whether focus following is enabled or not, based on
     * settings.a11y.screen_magnifier_focus_following preference.
     * @private {boolean}
     */
    this.screenMagnifierFocusFollowing_;

    /**
     * Whether magnifier is current initializing, and so should ignore
     * focus updates.
     * @private {boolean}
     */
    this.isInitializing_ = true;

    /**
     * Whether or not to draw a preview box around magnifier viewport area
     * instead of magnifying the screen for debugging.
     * @private {boolean}
     */
    this.magnifierDebugDrawRect_ = false;

    /**
     * Last seen mouse location (cached from event in onMouseMovedOrDragged).
     * @private {{x: number, y: number}}
     */
    this.mouseLocation_;

    /**
     * Last time mouse has moved (from last onMouseMovedOrDragged).
     * @private {Date}
     */
    this.lastMouseMovedTime_;

    /** @private {!EventHandler} */
    this.focusHandler_ = new EventHandler(
        [], EventType.FOCUS, event => this.onFocusOrSelectionChanged_(event));

    /** @private {!EventHandler} */
    this.activeDescendantHandler_ = new EventHandler(
        [], EventType.ACTIVE_DESCENDANT_CHANGED,
        event => this.onActiveDescendantChanged_(event));

    /** @private {!EventHandler} */
    this.selectionHandler_ = new EventHandler(
        [], EventType.SELECTION,
        event => this.onFocusOrSelectionChanged_(event));

    /** @private {!EventHandler} */
    this.onCaretBoundsChangedHandler = new EventHandler(
        [], EventType.CARET_BOUNDS_CHANGED,
        event => this.onCaretBoundsChanged(event));

    /** @private {!ChromeEventHandler} */
    this.onMagnifierBoundsChangedHandler_ = new ChromeEventHandler(
        chrome.accessibilityPrivate.onMagnifierBoundsChanged,
        bounds => this.onMagnifierBoundsChanged_(bounds));

    /** @private {ChromeEventHandler} */
    this.updateFromPrefsHandler_ = new ChromeEventHandler(
        chrome.settingsPrivate.onPrefsChanged,
        prefs => this.updateFromPrefs_(prefs));

    /** @private {!EventHandler} */
    this.onMouseMovedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_MOVED,
        event => this.onMouseMovedOrDragged_(event));

    /** @private {!EventHandler} */
    this.onMouseDraggedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_DRAGGED,
        event => this.onMouseMovedOrDragged_(event));

    /** @private {?function()} */
    this.onLoadDesktopCallbackForTest_ = null;

    this.init_();
  }

  /** Destructor to remove listener. */
  onMagnifierDisabled() {
    this.focusHandler_.stop();
    this.activeDescendantHandler_.stop();
    this.selectionHandler_.stop();
    this.onCaretBoundsChangedHandler.stop();
    this.onMagnifierBoundsChangedHandler_.stop();
    this.updateFromPrefsHandler_.stop();
    this.onMouseMovedHandler_.stop();
    this.onMouseDraggedHandler_.stop();
  }

  /**
   * Initializes Magnifier.
   * @private
   */
  init_() {
    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    this.updateFromPrefsHandler_.start();

    chrome.automation.getDesktop(desktop => {
      this.focusHandler_.setNodes(desktop);
      this.focusHandler_.start();
      this.activeDescendantHandler_.setNodes(desktop);
      this.activeDescendantHandler_.start();
      this.selectionHandler_.setNodes(desktop);
      this.selectionHandler_.start();
      this.onCaretBoundsChangedHandler.setNodes(desktop);
      this.onCaretBoundsChangedHandler.start();
      this.onMouseMovedHandler_.setNodes(desktop);
      this.onMouseMovedHandler_.start();
      this.onMouseDraggedHandler_.setNodes(desktop);
      this.onMouseDraggedHandler_.start();
      if (this.onLoadDesktopCallbackForTest_) {
        this.onLoadDesktopCallbackForTest_();
        this.onLoadDesktopCallbackForTest_ = null;
      }
    });

    this.onMagnifierBoundsChangedHandler_.start();

    chrome.accessibilityPrivate.enableMouseEvents(true);

    this.isInitializing_ = true;

    setTimeout(() => {
      this.isInitializing_ = false;
    }, Magnifier.IGNORE_FOCUS_UPDATES_INITIALIZATION_MS);

    chrome.commandLinePrivate.hasSwitch(
        'enable-magnifier-debug-draw-rect', enabled => {
          if (enabled) {
            this.magnifierDebugDrawRect_ = true;
          }
        });
  }

  /**
   * @param {!chrome.accessibilityPrivate.ScreenRect} bounds
   * @private
   */
  onMagnifierBoundsChanged_(bounds) {
    if (this.magnifierDebugDrawRect_) {
      chrome.accessibilityPrivate.setFocusRings([{
        rects: [bounds],
        type: chrome.accessibilityPrivate.FocusType.GLOW,
        color: '#22d',
      }]);
    }
  }

  /**
   * Sets |isInitializing_| inside tests to skip ignoring initial focus updates.
   */
  setIsInitializingForTest(isInitializing) {
    this.isInitializing_ = isInitializing;
  }

  /**
   * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
   * @private
   */
  updateFromPrefs_(prefs) {
    prefs.forEach(pref => {
      switch (pref.key) {
        case Magnifier.Prefs.SCREEN_MAGNIFIER_FOCUS_FOLLOWING:
          this.screenMagnifierFocusFollowing_ = Boolean(pref.value);
          break;
        default:
          return;
      }
    });
  }

  /**
   * Returns whether magnifier viewport should follow focus. Exposed for
   * testing.
   *
   * TODO(crbug.com/1146595): Add Chrome OS preference to allow disabling focus
   * following for docked magnifier.
   */
  shouldFollowFocus() {
    return !this.isInitializing_ &&
        (this.type === Magnifier.Type.DOCKED ||
         this.type === Magnifier.Type.FULL_SCREEN &&
             this.screenMagnifierFocusFollowing_);
  }

  /**
   * Listener for when focus is updated. Moves magnifier to include focused
   * element in viewport.
   *
   * TODO(accessibility): There is a bit of magnifier shakiness on arrow down in
   * omnibox - probably focus following fighting with caret following - maybe
   * add timer for last focus event so that fast-following caret updates don't
   * shake screen.
   * TODO(accessibility): On page load, sometimes viewport moves to center of
   * webpage instead of spotlighting first focusable page element.
   *
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onFocusOrSelectionChanged_(event) {
    const node = event.target;
    if (!node.location || !this.shouldFollowFocus()) {
      return;
    }

    if (new Date() - this.lastMouseMovedTime_ <
        Magnifier.IGNORE_FOCUS_UPDATES_AFTER_MOUSE_MOVE_MS) {
      return;
    }

    // Skip trying to move magnifier to encompass whole webpage or pdf. It's too
    // big, and magnifier usually ends up in middle at left edge of page.
    if (node.isRootNode || node.role === RoleType.WEB_VIEW ||
        node.role === RoleType.EMBEDDED_OBJECT) {
      return;
    }

    chrome.accessibilityPrivate.moveMagnifierToRect(node.location);
  }

  /**
   * Listener for when active descendant is changed. Moves magnifier to include
   * active descendant in viewport.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onActiveDescendantChanged_(event) {
    const {activeDescendant} = event.target;
    if (!activeDescendant || !this.shouldFollowFocus()) {
      return;
    }

    const {location} = activeDescendant;
    if (!location) {
      return;
    }

    chrome.accessibilityPrivate.moveMagnifierToRect(location);
  }

  /**
   * Listener for when caret bounds have changed. Moves magnifier to include
   * caret in viewport.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onCaretBoundsChanged(event) {
    const {target} = event;
    if (!target || !target.caretBounds) {
      return;
    }

    if (new Date() - this.lastMouseMovedTime_ <
        Magnifier.IGNORE_FOCUS_UPDATES_AFTER_MOUSE_MOVE_MS) {
      return;
    }

    // Note: onCaretBoundsChanged can get called when TextInputType is changed,
    // during which the caret bounds are set to an empty rect (0x0), and we
    // don't need to adjust the viewport position based on this bogus caret
    // position. This is only a transition period; the caret position will be
    // fixed upon focusing directly afterward.
    if (target.caretBounds.width === 0 && target.caretBounds.height === 0) {
      return;
    }

    const caretBoundsCenter = RectUtil.center(target.caretBounds);
    chrome.accessibilityPrivate.magnifierCenterOnPoint(caretBoundsCenter);
  }

  /**
   * Listener for when mouse moves or drags.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onMouseMovedOrDragged_(event) {
    this.lastMouseMovedTime_ = new Date();
    this.mouseLocation_ = {x: event.mouseX, y: event.mouseY};
  }

  /**
   * Used by C++ tests to ensure Magnifier load is competed.
   * @param {!function()} callback Callback for when desktop is loaded from
   * automation.
   */
  setOnLoadDesktopCallbackForTest(callback) {
    if (!this.focusHandler_.listening()) {
      this.onLoadDesktopCallbackForTest_ = callback;
      return;
    }
    // Desktop already loaded.
    callback();
  }
}

/**
 * Magnifier types.
 * @enum {string}
 * @const
 */
Magnifier.Type = {
  FULL_SCREEN: 'fullScreen',
  DOCKED: 'docked',
};

/**
 * Preferences that are configurable for Magnifier.
 * @enum {string}
 * @const
 */
Magnifier.Prefs = {
  SCREEN_MAGNIFIER_FOCUS_FOLLOWING:
      'settings.a11y.screen_magnifier_focus_following',
};

/**
 * Duration of time directly after startup of magnifier to ignore focus updates,
 * to prevent the magnified region from jumping.
 * @const {number}
 */
Magnifier.IGNORE_FOCUS_UPDATES_INITIALIZATION_MS = 500;

/**
 * Duration of time directly after a mouse move or drag to ignore focus updates,
 * to prevent the magnified region from jumping.
 * @const {number}
 */
Magnifier.IGNORE_FOCUS_UPDATES_AFTER_MOUSE_MOVE_MS = 250;
