// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationUtil} from '../common/automation_util.js';
import {ParagraphUtils} from '../common/paragraph_utils.js';

import {PrefsManager} from './prefs_manager.js';

const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const FocusRingStackingOrder =
    chrome.accessibilityPrivate.FocusRingStackingOrder;
const RoleType = chrome.automation.RoleType;
const SelectToSpeakPanelAction =
    chrome.accessibilityPrivate.SelectToSpeakPanelAction;

// This must match the name of view class that implements the SelectToSpeakTray:
// ash/system/accessibility/select_to_speak/select_to_speak_tray.h
export const SELECT_TO_SPEAK_TRAY_CLASS_NAME = 'SelectToSpeakTray';

// This must match the name of view class that implements the menu view:
// ash/system/accessibility/select_to_speak/select_to_speak_menu_view.h
const SELECT_TO_SPEAK_MENU_CLASS_NAME = 'SelectToSpeakMenuView';

// This must match the name of view class that implements the speed view:
// ash/system/accessibility/select_to_speak/select_to_speak_speed_view.h
const SELECT_TO_SPEAK_SPEED_CLASS_NAME = 'SelectToSpeakSpeedView';

// This must match the name of view class that implements the bubble views:
// ash/system/tray/tray_bubble_view.h
const TRAY_BUBBLE_VIEW_CLASS_NAME = 'TrayBubbleView';

// This must match the name of view class that implements the buttons used in
// the floating panel:
// ash/system/accessibility/floating_menu_button.h
const FLOATING_MENU_BUTTON_CLASS_NAME = 'FloatingMenuButton';

// A RGBA hex string for the default background shading color, which is black at
// 40% opacity (hex 66). This should be equivalent to using
// AshColorProvider::ShieldLayerType kShield40.
const DEFAULT_BACKGROUND_SHADING_COLOR = '#0006';

/**
 * Callbacks invoked when users perform actions in the UI.
 * @interface
 */
export class SelectToSpeakUiListener {
  /** User requests navigation to next paragraph. */
  onNextParagraphRequested() {}

  /** User requests navigation to previous paragraph. */
  onPreviousParagraphRequested() {}

  /** User requests navigation to next sentence. */
  onNextSentenceRequested() {}

  /** User requests navigation to previous sentence. */
  onPreviousSentenceRequested() {}

  /** User requests pausing TTS. */
  onPauseRequested() {}

  /** User requests resuming TTS. */
  onResumeRequested() {}

  /**
   * User requests reading speed adjustment.
   * @param {number} speed Speech rate multiplier.
   */
  onChangeSpeedRequested(speed) {}

  /** User requests exiting STS. */
  onExitRequested() {}

  /** User requests state change via tray button. */
  onStateChangeRequested() {}
}

/**
 * Manages user interface elements controlled by Select-to-speak, such the
 * focus ring, floating control panel, tray button, and word highlight.
 */
export class UiManager {
  /**
   * Please keep fields in alphabetical order.
   * @param {!PrefsManager} prefsManager
   * @param {!SelectToSpeakUiListener} listener
   */
  constructor(prefsManager, listener) {
    /** @private {?chrome.automation.AutomationNode} */
    this.desktop_ = null;

    /** @private {!SelectToSpeakUiListener} */
    this.listener_ = listener;

    /**
     * Button in the floating panel, useful for restoring focus to the panel.
     * @private {?chrome.automation.AutomationNode}
     */
    this.panelButton_ = null;

    /** @private {!PrefsManager} */
    this.prefsManager_ = prefsManager;

    this.init_();
  }

  /** @private */
  init_() {
    // Cache desktop and listen to focus changes.
    chrome.automation.getDesktop(desktop => {
      this.desktop_ = desktop;

      // Listen to focus changes so we can grab the floating panel when it
      // goes into focus, so it can be used later without having to search
      // through the entire tree.
      desktop.addEventListener(EventType.FOCUS, evt => {
        this.onFocusChange_(evt);
      }, true);
    });

    // Listen to panel events.
    chrome.accessibilityPrivate.onSelectToSpeakPanelAction.addListener(
        (panelAction, value) => {
          this.onPanelAction_(panelAction, value);
        });

    // Listen to event from activating tray button.
    chrome.accessibilityPrivate.onSelectToSpeakStateChangeRequested.addListener(
        () => {
          this.listener_.onStateChangeRequested();
        });
  }

  /**
   * Handles Select-to-speak panel action.
   * @param {!SelectToSpeakPanelAction} panelAction Action to perform.
   * @param {number=} value Optional value associated with action.
   * @private
   */
  onPanelAction_(panelAction, value) {
    switch (panelAction) {
      case SelectToSpeakPanelAction.NEXT_PARAGRAPH:
        this.listener_.onNextParagraphRequested();
        break;
      case SelectToSpeakPanelAction.PREVIOUS_PARAGRAPH:
        this.listener_.onPreviousParagraphRequested();
        break;
      case SelectToSpeakPanelAction.NEXT_SENTENCE:
        this.listener_.onNextSentenceRequested();
        break;
      case SelectToSpeakPanelAction.PREVIOUS_SENTENCE:
        this.listener_.onPreviousSentenceRequested();
        break;
      case SelectToSpeakPanelAction.EXIT:
        this.listener_.onExitRequested();
        break;
      case SelectToSpeakPanelAction.PAUSE:
        this.listener_.onPauseRequested();
        break;
      case SelectToSpeakPanelAction.RESUME:
        this.listener_.onResumeRequested();
        break;
      case SelectToSpeakPanelAction.CHANGE_SPEED:
        if (!value) {
          console.warn(
              'Change speed request receieved with invalid value', value);
          return;
        }
        this.listener_.onChangeSpeedRequested(value);
        break;
      default:
        console.warn('Unknown panel action received', panelAction);
    }
  }

  /**
   * Handles desktop-wide focus changes.
   * @param {!AutomationEvent} evt
   * @private
   */
  onFocusChange_(evt) {
    const focusedNode = evt.target;

    // As an optimization, look for the STS floating panel and store in case
    // we need to access that node at a later point (such as focusing panel).
    if (focusedNode.className !== FLOATING_MENU_BUTTON_CLASS_NAME) {
      // When panel is focused, initial focus is always on one of the buttons.
      return;
    }
    const windowParent =
        AutomationUtil.getFirstAncestorWithRole(focusedNode, RoleType.WINDOW);
    if (windowParent &&
        windowParent.className === TRAY_BUBBLE_VIEW_CLASS_NAME &&
        windowParent.children.length === 1 &&
        windowParent.children[0].className ===
            SELECT_TO_SPEAK_MENU_CLASS_NAME) {
      this.panelButton_ = focusedNode;
    }
  }

  /**
   * Sets focus to the floating control panel, if present.
   */
  setFocusToPanel() {
    // Used cached panel node if possible to avoid expensive desktop.find().
    // Note: Checking role attribute to see if node is still valid.
    if (this.panelButton_ && this.panelButton_.role) {
      // The panel itself isn't focusable, so set focus to most recently
      // focused panel button.
      this.panelButton_.focus();
      return;
    }
    this.panelButton_ = null;

    // Fallback to more expensive method of finding panel.
    if (!this.desktop_) {
      console.error('No cached desktop object, cannot focus panel');
      return;
    }
    const menuView = this.desktop_.find(
        {attributes: {className: SELECT_TO_SPEAK_MENU_CLASS_NAME}});
    if (menuView !== null && menuView.parent &&
        menuView.parent.className === TRAY_BUBBLE_VIEW_CLASS_NAME) {
      // The menu view's parent is the TrayBubbleView can can be assigned focus.
      this.panelButton_ = menuView.find({role: RoleType.TOGGLE_BUTTON});
      this.panelButton_.focus();
    }
  }

  /**
   * Sets the focus ring to |rects|. If |drawBackground|, draws the grey focus
   * background with the alpha set in prefs. |panelVisible| determines
   * the stacking order, so focus rings do not appear on top of panel.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @param {boolean} drawBackground
   * @param {boolean} panelVisible
   * @private
   */
  setFocusRings_(rects, drawBackground, panelVisible) {
    let color = '#0000';  // Fully transparent.
    if (drawBackground && this.prefsManager_.backgroundShadingEnabled()) {
      color = DEFAULT_BACKGROUND_SHADING_COLOR;
    }
    // If we're also showing a floating panel, ensure the focus ring appears
    // below the panel UI.
    const stackingOrder = panelVisible ?
        FocusRingStackingOrder.BELOW_ACCESSIBILITY_BUBBLES :
        FocusRingStackingOrder.ABOVE_ACCESSIBILITY_BUBBLES;
    chrome.accessibilityPrivate.setFocusRings([{
      rects,
      type: chrome.accessibilityPrivate.FocusType.GLOW,
      stackingOrder,
      color: this.prefsManager_.focusRingColor(),
      backgroundColor: color,
    }]);
  }

  /**
   * Updates the floating control panel.
   * @param {boolean} showPanel
   * @param {!chrome.accessibilityPrivate.ScreenRect=} anchorRect
   * @param {boolean=} paused
   * @param {number=} speechRateMultiplier
   * @private
   */
  updatePanel_(showPanel, anchorRect, paused, speechRateMultiplier) {
    if (showPanel) {
      if (anchorRect === undefined || paused === undefined ||
          speechRateMultiplier === undefined) {
        console.error('Cannot display panel: missing required parameters');
        return;
      }
      // If the feature is enabled and we have a valid focus ring, flip the
      // pause and resume button according to the current STS and TTS state.
      // Also, update the location of the panel according to the focus ring.
      chrome.accessibilityPrivate.updateSelectToSpeakPanel(
          /* show= */ true, /* anchor= */ anchorRect,
          /* isPaused= */ paused,
          /* speed= */ speechRateMultiplier);
    } else {
      // Dismiss the panel if either the feature is disabled or the focus ring
      // is not valid.
      chrome.accessibilityPrivate.updateSelectToSpeakPanel(/* show= */ false);
    }
  }

  /**
   * Updates word highlight.
   * @param {!AutomationNode} node Current node being spoken.
   * @param {?{start: number, end: number}} currentWord Character offsets of
   *    current word spoken within node if word highlighting is enabled.
   * @private
   */
  updateHighlight_(node, currentWord) {
    if (!currentWord) {
      chrome.accessibilityPrivate.setHighlights(
          [], this.prefsManager_.highlightColor());
      return;
    }
    // getStartCharIndexInParent is only defined for nodes with role
    // INLINE_TEXT_BOX.
    const charIndexInParent = node.role === RoleType.INLINE_TEXT_BOX ?
        ParagraphUtils.getStartCharIndexInParent(node) :
        0;
    node.boundsForRange(
        currentWord.start - charIndexInParent,
        currentWord.end - charIndexInParent, bounds => {
          const highlights = bounds ? [bounds] : [];
          chrome.accessibilityPrivate.setHighlights(
              highlights, this.prefsManager_.highlightColor());
        });
  }

  /**
   * Renders user selection rect, in the form of a focus ring.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   */
  setSelectionRect(rect) {
    // TODO(crbug.com/1185238): Support showing two focus rings at once, in case
    // a focus ring highlighting a node group is already present.
    this.setFocusRings_(
        [rect], false /* don't draw background */, false /* panelVisible */);
  }

  /**
   * Updates overlay UI based on current node and panel state.
   * @param {!ParagraphUtils.NodeGroup} nodeGroup Current node group.
   * @param {!AutomationNode} node Current node being spoken.
   * @param {?{start: number, end: number}} currentWord Character offsets of
   *    current word spoken within node if word highlighting is enabled.
   * @param {!{showPanel: boolean,
   *          paused: boolean,
   *          speechRateMultiplier: number}} panelState
   */
  update(nodeGroup, node, currentWord, panelState) {
    const {showPanel, paused, speechRateMultiplier} = panelState;
    // Show the block parent of the currently verbalized node with the
    // focus ring. If the node has no siblings in the group, highlight just
    // the one node.
    let focusRingRect;
    const currentBlockParent = nodeGroup.blockParent;
    if (currentBlockParent !== null && nodeGroup.nodes.length > 1) {
      focusRingRect = currentBlockParent.location;
    } else {
      focusRingRect = node.location;
    }
    this.updateHighlight_(node, currentWord);
    if (focusRingRect) {
      this.setFocusRings_(
          [focusRingRect], true /* draw background */, showPanel);
      this.updatePanel_(showPanel, focusRingRect, paused, speechRateMultiplier);
    } else {
      console.warn('No node location; cannot render focus ring or panel');
    }
  }

  /**
   * Clears overlay UI, hiding focus rings, panel, and word highlight.
   */
  clear() {
    this.setFocusRings_(
        [], false /* do not draw background */, false /* panel not visible */);
    chrome.accessibilityPrivate.setHighlights(
        [], this.prefsManager_.highlightColor());
    this.updatePanel_(false /* hide panel */);
  }

  /**
   * @param {?AutomationNode|undefined} node
   * @return {boolean} Whether given node is the Select-to-speak floating panel.
   */
  static isPanel(node) {
    if (!node) {
      return false;
    }

    // Determine if the node is part of the floating panel or the reading speed
    // selection bubble.
    return (
        node.className === TRAY_BUBBLE_VIEW_CLASS_NAME &&
        node.children.length === 1 &&
        (node.children[0].className === SELECT_TO_SPEAK_MENU_CLASS_NAME ||
         node.children[0].className === SELECT_TO_SPEAK_SPEED_CLASS_NAME));
  }

  /**
   * @param {?AutomationNode|undefined} node
   * @return {boolean} Whether given node is the Select-to-speak tray button.
   */
  static isTrayButton(node) {
    if (!node) {
      return false;
    }
    return AutomationUtil.getAncestors(node).find(n => {
      return n.className === SELECT_TO_SPEAK_TRAY_CLASS_NAME;
    }) !== undefined;
  }
}
