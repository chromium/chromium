// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '/common/rect_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {MenuManager} from './menu_manager.js';
import {SAChildNode, SANode} from './nodes/switch_access_node.js';
import {SwitchAccess} from './switch_access.js';
import {ErrorType, Mode} from './switch_access_constants.js';

import FocusRingInfo = chrome.accessibilityPrivate.FocusRingInfo;
import FocusType = chrome.accessibilityPrivate.FocusType;
import ScreenRect = chrome.accessibilityPrivate.ScreenRect;

type Observer = (primary: SANode | null, preview: SANode | null) => void;

/** Class to handle focus rings. */
export class FocusRingManager {
  private observer_?: Observer;
  /** A map of all the focus rings. */
  private rings_: Record<RingId, FocusRingInfo>;
  private ringNodesForTesting_: Record<RingId, SANode | null> = {
    [RingId.PRIMARY]: null,
    [RingId.PREVIEW]: null,
  };

  private static instance_?: FocusRingManager;

  private constructor() {
    this.rings_ = this.createRings_();
  }

  static init(): void {
    if (FocusRingManager.instance_) {
      throw SwitchAccess.error(
          ErrorType.DUPLICATE_INITIALIZATION,
          'Cannot initialize focus ring manager twice.');
    }
    FocusRingManager.instance_ = new FocusRingManager();
  }

  static get instance(): FocusRingManager {
    if (!FocusRingManager.instance_) {
      throw SwitchAccess.error(
          ErrorType.UNINITIALIZED,
          'FocusRingManager cannot be accessed before being initialized');
    }
    return FocusRingManager.instance_;
  }

  /** Sets the focus ring color. */
  static setColor(color: string): void {
    if (!COLOR_PATTERN.test(color)) {
      console.error(SwitchAccess.error(
          ErrorType.INVALID_COLOR,
          'Problem setting focus ring color: ' + color + ' is not' +
              'a valid CSS color string.'));
      return;
    }
    FocusRingManager.instance.setColorValidated_(color);
  }

  /** Sets the primary and preview focus rings based on the provided node. */
  static setFocusedNode(node: SAChildNode): void {
    if (node.ignoreWhenComputingUnionOfBoundingBoxes()) {
      FocusRingManager.instance.setFocusedNodeIgnorePrimary_(node);
      return;
    }

    if (!node.location) {
      throw SwitchAccess.error(
          ErrorType.MISSING_LOCATION,
          'Cannot set focus rings if node location is undefined',
          true /* shouldRecover */);
    }

    // If the primary node is a group, show its first child as the "preview"
    // focus.
    if (node.isGroup()) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      const firstChild = node.asRootNode()!.firstChild;
      FocusRingManager.instance.setFocusedNodeGroup_(node, firstChild);
      return;
    }

    FocusRingManager.instance.setFocusedNodeLeaf_(node);
  }

  /** Clears all focus rings. */
  static clearAll(): void {
    FocusRingManager.instance.clearAll_();
  }

  /**
   * Set an observer that will be called every time the focus rings
   * are updated. It will be called with two arguments: the node for
   * the primary ring, and the node for the preview ring. Either may
   * be null.
   */
  static setObserver(observer: Observer): void {
    FocusRingManager.instance.observer_ = observer;
  }

  // ======== Private methods ========

  private clearAll_(): void {
    this.forEachRing_(ring => ring.rects = []);
    this.updateNodesForTesting_(null, null);
    this.updateFocusRings_();
  }

  /** Creates the map of focus rings. */
  private createRings_(): Record<RingId, FocusRingInfo> {
    const primaryRing = {
      id: RingId.PRIMARY,
      rects: [],
      type: FocusType.SOLID,
      color: PRIMARY_COLOR,
      secondaryColor: OUTER_COLOR,
    };

    const previewRing = {
      id: RingId.PREVIEW,
      rects: [],
      type: FocusType.DASHED,
      color: PREVIEW_COLOR,
      secondaryColor: OUTER_COLOR,
    };

    return {
      [RingId.PRIMARY]: primaryRing,
      [RingId.PREVIEW]: previewRing,
    };
  }

  /** Calls a function for each focus ring. */
  private forEachRing_(callback: (info: FocusRingInfo) => void): void {
    Object.values(this.rings_).forEach(ring => callback(ring));
  }

  /** Sets the focus ring color. Assumes the color has been validated. */
  private setColorValidated_(color: string): void {
    this.forEachRing_(ring => ring.color = color);
  }

  /**
   * Sets the primary focus ring to |node|, and the preview focus ring to
   * |firstChild|.
   */
  private setFocusedNodeGroup_(
      group: SAChildNode, firstChild: SAChildNode): void {
    // Clear the dashed ring between transitions, as the animation is
    // distracting.
    this.rings_[RingId.PREVIEW].rects = [];

    let focusRect: ScreenRect = group.location!;
    const childRect = firstChild ? firstChild.location : null;
    if (childRect) {
      // If the current element is not specialized in location handling, e.g.
      // the back button, the focus rect should expand to contain the child
      // rect.
      focusRect =
          RectUtil.expandToFitWithPadding(GROUP_BUFFER, focusRect, childRect)!;
      this.rings_[RingId.PREVIEW].rects = [childRect];
    }
    this.rings_[RingId.PRIMARY].rects = [focusRect];
    this.updateNodesForTesting_(group, firstChild);
    this.updateFocusRings_();
  }

  /**
   * Clears the primary focus ring and sets the preview focus ring based on the
   * provided node.
   */
  private setFocusedNodeIgnorePrimary_(node: SAChildNode): void {
    // Nodes of this type, e.g. the back button node, handles setting its own
    // focus, as it has special requirements (a round focus ring that has no
    // gap with the edges of the view).
    this.rings_[RingId.PRIMARY].rects = [];
    // Clear the dashed ring between transitions, as the animation is
    // distracting.
    this.rings_[RingId.PREVIEW].rects = [];
    this.updateFocusRings_();

    // Show the preview focus ring unless the menu is open (it has a custom exit
    // button).
    if (!MenuManager.isMenuOpen()) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      this.rings_[RingId.PREVIEW].rects = [node.group!.location];
    }
    this.updateNodesForTesting_(node, node.group);
    this.updateFocusRings_();
  }

  /** Sets the primary focus to |node| and clears the secondary focus. */
  private setFocusedNodeLeaf_(node: SAChildNode): void {
    // TODO(b/314203187): Not nulls asserted, check these to make sure
    // this is correct.
    this.rings_[RingId.PRIMARY].rects = [node.location!];
    this.rings_[RingId.PREVIEW].rects = [];
    this.updateNodesForTesting_(node, null);
    this.updateFocusRings_();
  }

  /**
   * Updates all focus rings to reflect new location, color, style, or other
   * changes. Enables observers to monitor what's focused.
   */
  private updateFocusRings_(): void {
    if (SwitchAccess.mode === Mode.POINT_SCAN && !MenuManager.isMenuOpen()) {
      return;
    }

    const focusRings = Object.values(this.rings_);
    chrome.accessibilityPrivate.setFocusRings(
        focusRings,
        chrome.accessibilityPrivate.AssistiveTechnologyType.SWITCH_ACCESS);
  }

  /** Saves the primary/preview focus for testing. */
  private updateNodesForTesting_(
      primary: SANode | null, preview: SANode | null): void {
    // Keep track of the nodes associated with each focus ring for testing
    // purposes, since focus ring locations are not guaranteed to exactly match
    // node locations.
    this.ringNodesForTesting_[RingId.PRIMARY] = primary;
    this.ringNodesForTesting_[RingId.PREVIEW] = preview;

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
 */
const COLOR_PATTERN = /^#([0-9A-F]{3,4}|[0-9A-F]{6}|[0-9A-F]{8})$/i;

/**
 * The buffer (in dip) between a child's focus ring and its parent's focus
 * ring.
 */
const GROUP_BUFFER = 2;

/**
 * The focus ring IDs used by Switch Access.
 * Exported for testing.
 */
export enum RingId {
  // The ID for the ring showing the user's current focus.
  PRIMARY = 'primary',
  // The ID for the ring showing a preview of the next focus, if the user
  // selects the current element.
  PREVIEW = 'preview',
}

/** The secondary color for both rings. */
const OUTER_COLOR = '#174EA6';  // Google Blue 900

/** The inner color of the preview focus ring. */
const PREVIEW_COLOR = '#8AB4F880';  // Google Blue 300, 50% opacity

/** The inner color of the primary focus ring. */
const PRIMARY_COLOR = '#8AB4F8';  // Google Blue 300

TestImportManager.exportForTesting(FocusRingManager, ['RingId', RingId]);
