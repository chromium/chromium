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
    return this.type === Magnifier.Type.DOCKED ||
        this.type === Magnifier.Type.FULL_SCREEN &&
        this.screenMagnifierFocusFollowing_;
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
