// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Main class for the Chrome OS magnifier.
 */
class Magnifier {
  constructor() {
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
    chrome.automation.getDesktop(desktop => {
      this.focusHandler_.setNodes(desktop);
      this.focusHandler_.start();
      this.activeDescendantHandler_.setNodes(desktop);
      this.activeDescendantHandler_.start();
    });
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
    if (!location) {
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
    if (!activeDescendant) {
      return;
    }

    const {location} = activeDescendant;
    if (!location) {
      return;
    }

    chrome.accessibilityPrivate.moveMagnifierToRect(location);
  }
}
