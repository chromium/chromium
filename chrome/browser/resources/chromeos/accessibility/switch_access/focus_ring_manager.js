// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '../common/rect_util.js';

import {MenuManager} from './menu_manager.js';
import {SAChildNode, SANode, SARootNode} from './nodes/switch_access_node.js';
import {SwitchAccess} from './switch_access.js';
import {SAConstants} from './switch_access_constants.js';

const FocusRingInfo = chrome.accessibilityPrivate.FocusRingInfo;
const FocusType = chrome.accessibilityPrivate.FocusType;


/** Class to handle focus rings. */
export class FocusRingManager {
  /** @private */
  constructor() {
    /**
     * A map of all the focus rings.
     * @private {!Object<SAConstants.Focus.ID, FocusRingInfo>}
     */
    this.rings_ = this.createRings_();

    /** @private {!Object<SAConstants.Focus.ID, ?SANode>} */
    this.ringNodesForTesting_ = {
      [SAConstants.Focus.ID.PRIMARY]: null,
      [SAConstants.Focus.ID.PREVIEW]: null,
    };
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
    if (!FocusRingManager.colorPattern_.test(color)) {
      console.error(SwitchAccess.error(
          SAConstants.ErrorType.INVALID_COLOR,
          'Problem setting focus ring color: ' + color + ' is not' +
              'a valid CSS color string.'));
      return;
    }
    FocusRingManager.instance.setColorValidated_(color);
  }

  /**
   * Sets the primary and preview focus rings based on the provided node.
   * @param {!SAChildNode} node
   */
  static setFocusedNode(node) {
    if (node.ignoreWhenComputingUnionOfBoundingBoxes()) {
      FocusRingManager.instance.setFocusedNodeIgnorePrimary_(node);
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
      FocusRingManager.instance.setFocusedNodeGroup_(node, firstChild);
      return;
    }

    FocusRingManager.instance.setFocusedNodeLeaf_(node);
  }

  /** Clears all focus rings. */
  static clearAll() {
    FocusRingManager.instance.clearAll_();
  }

  /**
   * Set an observer that will be called every time the focus rings
   * are updated. It will be called with two arguments: the node for
   * the primary ring, and the node for the preview ring. Either may
   * be null.
   * @param {function(?SANode, ?SANode)} observer
   */
  static setObserver(observer) {
    FocusRingManager.instance.observer_ = observer;
  }

  // ======== Private methods ========

  /** @private */
  clearAll_() {
    this.forEachRing_(ring => ring.rects = []);
    this.updateNodesForTesting_(null, null);
    this.updateFocusRings_();
  }

  /**
   * Creates the map of focus rings.
   * @return {!Object<SAConstants.Focus.ID, FocusRingInfo>}
   * @private
   */
  createRings_() {
    const primaryRing = {
      id: SAConstants.Focus.ID.PRIMARY,
      rects: [],
      type: FocusType.SOLID,
      color: SAConstants.Focus.PRIMARY_COLOR,
      secondaryColor: SAConstants.Focus.OUTER_COLOR,
    };

    const previewRing = {
      id: SAConstants.Focus.ID.PREVIEW,
      rects: [],
      type: FocusType.DASHED,
      color: SAConstants.Focus.PREVIEW_COLOR,
      secondaryColor: SAConstants.Focus.OUTER_COLOR,
    };

    return {
      [SAConstants.Focus.ID.PRIMARY]: primaryRing,
      [SAConstants.Focus.ID.PREVIEW]: previewRing,
    };
  }

  /**
   * Calls a function for each focus ring.
   * @param {!function(!FocusRingInfo)} callback
   * @private
   */
  forEachRing_(callback) {
    Object.values(this.rings_).forEach(ring => callback(ring));
  }

  /**
   * Sets the focus ring color. Assumes the color has already been validated.
   * @param {!string} color
   * @private
   */
  setColorValidated_(color) {
    this.forEachRing_(ring => ring.color = color);
  }

  /**
   * Sets the primary focus ring to |node|, and the preview focus ring to
   * |firstChild|.
   * @param {!SAChildNode} group
   * @param {!SAChildNode} firstChild
   * @private
   */
  setFocusedNodeGroup_(group, firstChild) {
    // Clear the dashed ring between transitions, as the animation is
    // distracting.
    this.rings_[SAConstants.Focus.ID.PREVIEW].rects = [];

    let focusRect = group.location;
    const childRect = firstChild ? firstChild.location : null;
    if (childRect) {
      // If the current element is not specialized in location handling, e.g.
      // the back button, the focus rect should expand to contain the child
      // rect.
      focusRect = RectUtil.expandToFitWithPadding(
          SAConstants.Focus.GROUP_BUFFER, focusRect, childRect);
      this.rings_[SAConstants.Focus.ID.PREVIEW].rects = [childRect];
    }
    this.rings_[SAConstants.Focus.ID.PRIMARY].rects = [focusRect];
    this.updateNodesForTesting_(group, firstChild);
    this.updateFocusRings_();
  }

  /**
   * Clears the primary focus ring and sets the preview focus ring based on the
   *     provided node.
   * @param {!SAChildNode} node
   * @private
   */
  setFocusedNodeIgnorePrimary_(node) {
    // Nodes of this type, e.g. the back button node, handles setting its own
    // focus, as it has special requirements (a round focus ring that has no
    // gap with the edges of the view).
    this.rings_[SAConstants.Focus.ID.PRIMARY].rects = [];
    // Clear the dashed ring between transitions, as the animation is
    // distracting.
    this.rings_[SAConstants.Focus.ID.PREVIEW].rects = [];
    this.updateFocusRings_();

    // Show the preview focus ring unless the menu is open (it has a custom exit
    // button).
    if (!MenuManager.isMenuOpen()) {
      this.rings_[SAConstants.Focus.ID.PREVIEW].rects = [node.group.location];
    }
    this.updateNodesForTesting_(node, node.group);
    this.updateFocusRings_();
  }

  /**
   * Sets the primary focus to |node| and clears the secondary focus.
   * @param {!SAChildNode} node
   * @private
   */
  setFocusedNodeLeaf_(node) {
    this.rings_[SAConstants.Focus.ID.PRIMARY].rects = [node.location];
    this.rings_[SAConstants.Focus.ID.PREVIEW].rects = [];
    this.updateNodesForTesting_(node, null);
    this.updateFocusRings_();
  }

  /**
   * Updates all focus rings to reflect new location, color, style, or other
   * changes. Enables observers to monitor what's focused.
   * @private
   */
  updateFocusRings_() {
    if (SwitchAccess.mode === SAConstants.Mode.POINT_SCAN &&
        !MenuManager.isMenuOpen()) {
      return;
    }

    const focusRings = Object.values(this.rings_);
    chrome.accessibilityPrivate.setFocusRings(focusRings);
  }

  /**
   * Saves the primary/preview focus for testing.
   * @param {?SANode} primary
   * @param {?SANode} preview
   * @private
   */
  updateNodesForTesting_(primary, preview) {
    // Keep track of the nodes associated with each focus ring for testing
    // purposes, since focus ring locations are not guaranteed to exactly match
    // node locations.
    this.ringNodesForTesting_[SAConstants.Focus.ID.PRIMARY] = primary;
    this.ringNodesForTesting_[SAConstants.Focus.ID.PREVIEW] = preview;

    const observer = FocusRingManager.instance.observer_;
    if (observer) {
      observer(primary, preview);
    }
  }
}

/**
 * Regex pattern to verify valid colors. Checks that the first character
 * is '#', followed by 3, 4, 6, or 8 valid hex characters, and no other
 * characters (ignoring case).
 * @private {RegExp}
 */
FocusRingManager.colorPattern_ = /^#([0-9A-F]{3,4}|[0-9A-F]{6}|[0-9A-F]{8})$/i;
