// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    /** @private {!EventHandler} */
    this.focusHandler_ = new EventHandler(
        [], chrome.automation.EventType.FOCUS, this.onFocus_.bind(this));

    this.activeDescendantHandler_ = new EventHandler(
        [], chrome.automation.EventType.ACTIVE_DESCENDANT_CHANGED,
        this.onActiveDescendantChanged_.bind(this));

    this.init_();
  }

  /** Destructor to remove listener. */
  onMagnifierDisabled() {
    this.focusHandler_.stop();
    this.activeDescendantHandler_.stop();

    chrome.accessibilityPrivate.onMagnifierBoundsChanged.removeListener(
        this.onMagnifierBoundsChanged_);
  }

  /**
   * Initializes Magnifier.
   * @private
   */
  init_() {
    chrome.settingsPrivate.getAllPrefs(this.updateFromPrefs_.bind(this));
    chrome.settingsPrivate.onPrefsChanged.addListener(
        this.updateFromPrefs_.bind(this));

    chrome.automation.getDesktop(desktop => {
      this.focusHandler_.setNodes(desktop);
      this.focusHandler_.start();
      this.activeDescendantHandler_.setNodes(desktop);
      this.activeDescendantHandler_.start();
    });

    chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
        this.onMagnifierBoundsChanged_.bind(this));

    this.isInitializing_ = true;

    setTimeout(() => {
      this.isInitializing_ = false;
    }, Magnifier.IGNORE_FOCUS_UPDATES_INITIALIZATION_MS);

    chrome.commandLinePrivate.hasSwitch(
        'enable-magnifier-debug-draw-rect', (enabled) => {
          if (enabled) {
            this.magnifierDebugDrawRect_ = true;
          }
        });
  }

  onMagnifierBoundsChanged_(bounds) {
    if (this.magnifierDebugDrawRect_) {
      chrome.accessibilityPrivate.setFocusRings([{
        rects: [bounds],
        type: chrome.accessibilityPrivate.FocusType.GLOW,
        color: '#22d'
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
    prefs.forEach((pref) => {
      switch (pref.key) {
        case Magnifier.Prefs.SCREEN_MAGNIFIER_FOCUS_FOLLOWING:
          this.screenMagnifierFocusFollowing_ = !!pref.value;
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
  onFocus_(event) {
    const {location} = event.target;
    if (!location || !this.shouldFollowFocus()) {
      return;
    }

    chrome.accessibilityPrivate.moveMagnifierToRect(location);
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
