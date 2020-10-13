// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Main class for the Chrome OS magnifier.
 */
class Magnifier {
  constructor() {
    /** @private {!EventHandler} */
    this.activeDescendantHandler_ = new EventHandler(
        [], chrome.automation.EventType.ACTIVE_DESCENDANT_CHANGED,
        this.onActiveDescendantChanged_.bind(this));

    this.init_();
  }

  /** Destructor to remove listener. */
  onMagnifierDisabled() {
    this.activeDescendantHandler_.stop();
  }

  /**
   * Initializes Magnifier.
   * @private
   */
  init_() {
    chrome.automation.getDesktop(desktop => {
      this.activeDescendantHandler_.setNodes(desktop);
      this.activeDescendantHandler_.start();
    });
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
