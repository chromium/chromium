// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuManager} from './menu_manager.js';
import {SAChildNode, SARootNode} from './nodes/switch_access_node.js';
import {SwitchAccess} from './switch_access.js';
import {SAConstants} from './switch_access_constants.js';

/**
 * Class to handle focus rings.
 */
export class FocusRingManager {
  /** @private */
  constructor() {
    /**
     * A map of all the focus rings.
     * @private {!Map<SAConstants.Focus.ID,
     *     chrome.accessibilityPrivate.FocusRingInfo>}
     */
    this.rings_ = this.createMap_();

    /**
     * Regex pattern to verify valid colors. Checks that the first character
     * is '#', followed by 3, 4, 6, or 8 valid hex characters, and no other
     * characters (ignoring case).
     * @private
     */
    this.colorPattern_ = /^#([0-9A-F]{3,4}|[0-9A-F]{6}|[0-9A-F]{8})$/i;
  }

  static get instance() {
    if (!FocusRingManager.instance_) {
      FocusRingManager.instance_ = new FocusRingManager();
    }
    return FocusRingManager.instance_;
  }

  /**
   * Sets the focus ring color.
   * @param {!string} color
   */
  static setColor(color) {
    const manager = FocusRingManager.instance;

    if (manager.colorPattern_.test(color) !== true) {
      console.error(SwitchAccess.error(
          SAConstants.ErrorType.INVALID_COLOR,
          'Problem setting focus ring color: ' + color + ' is not' +
              'a valid CSS color string.'));
      return;
    }
    manager.rings_.forEach((ring) => ring.color = color);
  }

  /**
   * Sets the primary and preview focus rings based on the current primary and
   *     group nodes used for navigation.
   * @param {!SAChildNode} node
   */
  static setFocusedNode(node) {
    const manager = FocusRingManager.instance;

    if (node.ignoreWhenComputingUnionOfBoundingBoxes()) {
      // Nodes of this type, e.g. the back button node, handles setting its own
      // focus, as it has special requirements (a round focus ring that has no
      // gap with the edges of the view).
      manager.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [];
      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      manager.rings_.get(SAConstants.Focus.ID.PREVIEW).rects = [];
      manager.updateFocusRings_(node, null);

      // The dashed focus ring should not be shown around the menu when exiting.
      if (!MenuManager.isMenuOpen()) {
        manager.rings_.get(SAConstants.Focus.ID.PREVIEW).rects =
            [node.group.location];
        manager.updateFocusRings_(node, null);
      }
      return;
    }

    if (!node.location) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.MISSING_LOCATION,
          'Cannot set focus rings if node location is undefined',
          true /* shouldRecover */);
    }

    // If the primary node is a group, show its first child as the "preview"
    // focus.
    if (node.isGroup()) {
      const firstChild = node.asRootNode().firstChild;

      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      manager.rings_.get(SAConstants.Focus.ID.PREVIEW).rects = [];

      let focusRect = node.location;
      const childRect = firstChild ? firstChild.location : null;
      if (childRect) {
        // If the current element is not specialized in location handling, e.g.
        // the back button, the focus rect should expand to contain the child
        // rect.
        focusRect = RectUtil.expandToFitWithPadding(
            SAConstants.Focus.GROUP_BUFFER, focusRect, childRect);
        manager.rings_.get(SAConstants.Focus.ID.PREVIEW).rects = [childRect];
      }
      manager.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [focusRect];
      manager.updateFocusRings_(node, firstChild);
      return;
    }

    manager.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [node.location];
    manager.rings_.get(SAConstants.Focus.ID.PREVIEW).rects = [];
    manager.updateFocusRings_(node, null);
  }

  /** Clears all focus rings. */
  static clearAll() {
    const manager = FocusRingManager.instance;
    manager.rings_.forEach((ring) => {
      ring.rects = [];
    });
    manager.updateFocusRings_(null, null);
  }

  /**
   * Set an observer that will be called every time the focus rings
   * are updated. It will be called with two arguments: the node for
   * the primary ring, and the node for the preview ring. Either may
   * be null.
   * @param {function(SAChildNode, SAChildNode)} observer
   */
  static setObserver(observer) {
    FocusRingManager.instance.observer_ = observer;
  }

  /**
   * Creates the map of focus rings.
   * @return {!Map<SAConstants.Focus.ID,
   * chrome.accessibilityPrivate.FocusRingInfo>}
   * @private
   */
  createMap_() {
    const primaryRing = {
      id: SAConstants.Focus.ID.PRIMARY,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.SOLID,
      color: SAConstants.Focus.PRIMARY_COLOR,
      secondaryColor: SAConstants.Focus.OUTER_COLOR
    };

    const previewRing = {
      id: SAConstants.Focus.ID.PREVIEW,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: SAConstants.Focus.PREVIEW_COLOR,
      secondaryColor: SAConstants.Focus.OUTER_COLOR
    };

    return new Map([
      [SAConstants.Focus.ID.PRIMARY, primaryRing],
      [SAConstants.Focus.ID.PREVIEW, previewRing]
    ]);
  }


  /**
   * Updates all focus rings to reflect new location, color, style, or other
   * changes. Enables observers to monitor what's focused.
   * @private
   */
  updateFocusRings_(primaryRingNode, previewRingNode) {
    const focusRings = [];
    this.rings_.forEach((ring) => focusRings.push(ring));
    chrome.accessibilityPrivate.setFocusRings(focusRings);

    const observer = FocusRingManager.instance.observer_;
    if (observer) {
      observer(primaryRingNode, previewRingNode);
    }
  }
}
