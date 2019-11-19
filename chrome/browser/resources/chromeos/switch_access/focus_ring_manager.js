// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle focus rings.
 */
class FocusRingManager {
  constructor() {
    /**
     * A map of all the focus rings.
     * @private {!Map<SAConstants.Focus.ID,
     *     chrome.accessibilityPrivate.FocusRingInfo>}
     */
    this.rings_ = new Map();

    /**
     * Regex pattern to verify valid colors. Checks that the first character
     * is '#', followed by between 3 and 8 valid hex characters, and no other
     * characters (ignoring case).
     */
    this.colorPattern_ = /^#[0-9A-F]{3,8}$/i;

    /**
     * Reference to the menu panel object.
     * @private {PanelInterface}
     */
    this.menuPanel_;
  }

  /** @param {!PanelInterface} panel */
  setMenuPanel(panel) {
    this.menuPanel_ = panel;
  }

  /** Finishes setup of focus rings once the preferences are loaded. */
  onPrefsReady() {
    // Currently all focus rings share the same color.
    // TODO(crbug/996852): Make the primary focus color a preference.
    const color = SAConstants.Focus.PRIMARY_COLOR;

    // Create each focus ring.
    this.rings_.set(SAConstants.Focus.ID.PRIMARY, {
      id: SAConstants.Focus.ID.PRIMARY,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.SOLID,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
    this.rings_.set(SAConstants.Focus.ID.NEXT, {
      id: SAConstants.Focus.ID.NEXT,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
    this.rings_.set(SAConstants.Focus.ID.TEXT, {
      id: SAConstants.Focus.ID.TEXT,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
  }

  /**
   * Sets the focus ring color.
   * @param {!string} color
   */
  setColor(color) {
    if (this.colorPattern_.test(color) !== true) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.INVALID_COLOR,
          'Problem setting focus ring color: color is not' +
              'a valid CSS color string.');
    }
    this.rings_.forEach((ring) => ring.color = color);
  }

  /**
   * Sets the primary and next focus rings based on the current primary and
   *     group nodes used for navigation.
   * @param {!SAChildNode} primary
   * @param {!SARootNode} group
   */
  setFocusNodes(primary, group) {
    if (this.rings_.size === 0) {
      return;
    }
    if (!primary.location || !group.location) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.MISSING_LOCATION,
          'Cannot set focus rings if node location is undefined');
    }

    if (primary instanceof BackButtonNode) {
      // TODO(anastasi): Use standard focus rings.
      if (this.menuPanel_) {
        this.menuPanel_.setFocusRing(SAConstants.BACK_ID, true);
      }
      this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [];
      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
      this.updateFocusRings_();

      this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [group.location];
      this.updateFocusRings_();
      return;
    } else if (this.menuPanel_) {
      this.menuPanel_.setFocusRing(SAConstants.BACK_ID, false);
    }

    // If the primary node is a group, show its first child as the "next" focus.
    if (primary.isGroup()) {
      const firstChild = primary.asRootNode().firstChild;

      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
      this.updateFocusRings_();

      let focusRect = primary.location;
      let childRect = firstChild ? firstChild.location : null;
      if (childRect) {
        // If the current element is not the back button, the focus rect should
        // expand to contain the child rect.
        focusRect = RectHelper.expandToFitWithPadding(
            SAConstants.Focus.GROUP_BUFFER, focusRect, childRect);
        this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [childRect];
      }
      this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [focusRect];
      this.updateFocusRings_();
      return;
    }

    this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [primary.location];
    this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
    this.updateFocusRings_();
  }

  /**
   * Clears all focus rings.
   */
  clearAll() {
    this.rings_.forEach((ring) => ring.rects = []);
    this.updateFocusRings_();
  }

  /**
   * Updates all focus rings to reflect new location, color, style, or other
   * changes.
   */
  updateFocusRings_() {
    let focusRings = [];
    this.rings_.forEach((ring) => focusRings.push(ring));
    chrome.accessibilityPrivate.setFocusRings(focusRings);
  }
}
