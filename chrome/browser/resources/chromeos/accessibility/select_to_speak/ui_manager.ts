// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationUtil} from '/common/automation_util.js';
import {ParagraphUtils} from '/common/paragraph_utils.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {PrefsManager} from './prefs_manager.js';

type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const FocusRingStackingOrder =
    chrome.accessibilityPrivate.FocusRingStackingOrder;

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
 */
export interface SelectToSpeakUiListener {
  /** User requests navigation to next paragraph. */
  onNextParagraphRequested: () => void;

  /** User requests navigation to previous paragraph. */
  onPreviousParagraphRequested: () => void;

  /** User requests navigation to next sentence. */
  onNextSentenceRequested: () => void;

  /** User requests navigation to previous sentence. */
  onPreviousSentenceRequested: () => void;

  /** User requests pausing TTS. */
  onPauseRequested: () => void;

  /** User requests resuming TTS. */
  onResumeRequested: () => void;

  /**
   * User requests reading speed adjustment.
   * @param speed rate multiplier.
   */
  onChangeSpeedRequested: (speed: number) => void;

  /** User requests exiting STS. */
  onExitRequested: () => void;

  /** User requests state change via tray button. */
  onStateChangeRequested: () => void;
}

/**
 * Manages user interface elements controlled by Select-to-speak, such the
 * focus ring, floating control panel, tray button, and word highlight.
 */
export class UiManager {
  // TODO(b/314204374): Convert from null to undefined.
  private desktop_: AutomationNode|null;
  private listener_: SelectToSpeakUiListener;
  // TODO(b/314204374): Convert from null to undefined.
  private panelButton_: AutomationNode|null;
  private prefsManager_: PrefsManager;
  /**
   * Please keep fields in alphabetical order.
   */
  constructor(prefsManager: PrefsManager, listener: SelectToSpeakUiListener) {
    this.desktop_ = null;
    this.listener_ = listener;

    /**
     * Button in the floating panel, useful for restoring focus to the panel.
     */
    this.panelButton_ = null;
    this.prefsManager_ = prefsManager;

    this.init_();
  }

  private init_(): void {
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
   * @param panelAction Action to perform.
   * @param value Optional value associated with action.
   */
  private onPanelAction_(
      panelAction: chrome.accessibilityPrivate.SelectToSpeakPanelAction,
      value?: number): void {
    switch (panelAction) {
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction.NEXT_PARAGRAPH:
        this.listener_.onNextParagraphRequested();
        break;
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction
          .PREVIOUS_PARAGRAPH:
        this.listener_.onPreviousParagraphRequested();
        break;
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction.NEXT_SENTENCE:
        this.listener_.onNextSentenceRequested();
        break;
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction
          .PREVIOUS_SENTENCE:
        this.listener_.onPreviousSentenceRequested();
        break;
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction.EXIT:
        this.listener_.onExitRequested();
        break;
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction.PAUSE:
        this.listener_.onPauseRequested();
        break;
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction.RESUME:
        this.listener_.onResumeRequested();
        break;
      case chrome.accessibilityPrivate.SelectToSpeakPanelAction.CHANGE_SPEED:
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
   */
  private onFocusChange_(evt: chrome.automation.AutomationEvent): void {
    const focusedNode = evt.target;

    // As an optimization, look for the STS floating panel and store in case
    // we need to access that node at a later point (such as focusing panel).
    if (focusedNode.className !== FLOATING_MENU_BUTTON_CLASS_NAME) {
      // When panel is focused, initial focus is always on one of the buttons.
      return;
    }
    const windowParent = AutomationUtil.getFirstAncestorWithRole(
        focusedNode, chrome.automation.RoleType.WINDOW);
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
  setFocusToPanel(): void {
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
      this.panelButton_ =
          menuView.find({role: chrome.automation.RoleType.TOGGLE_BUTTON});
      this.panelButton_.focus();
    }
  }

  /**
   * Sets the focus ring to |rects|. If |drawBackground|, draws the grey focus
   * background with the alpha set in prefs. |panelVisible| determines
   * the stacking order, so focus rings do not appear on top of panel.
   */
  private setFocusRings_(
      rects: chrome.accessibilityPrivate.ScreenRect[], drawBackground: boolean,
      panelVisible: boolean): void {
    let color = '#0000';  // Fully transparent.
    if (drawBackground && this.prefsManager_.backgroundShadingEnabled()) {
      color = DEFAULT_BACKGROUND_SHADING_COLOR;
    }
    // If we're also showing a floating panel, ensure the focus ring appears
    // below the panel UI.
    const stackingOrder = panelVisible ?
        FocusRingStackingOrder.BELOW_ACCESSIBILITY_BUBBLES :
        FocusRingStackingOrder.ABOVE_ACCESSIBILITY_BUBBLES;
    chrome.accessibilityPrivate.setFocusRings(
        [{
          rects,
          type: chrome.accessibilityPrivate.FocusType.GLOW,
          stackingOrder,
          color: this.prefsManager_.focusRingColor(),
          backgroundColor: color,
        }],
        chrome.accessibilityPrivate.AssistiveTechnologyType.SELECT_TO_SPEAK);
  }

  /**
   * Updates the floating control panel.
   */
  private updatePanel_(
      showPanel: boolean, anchorRect?: chrome.accessibilityPrivate.ScreenRect,
      paused?: boolean, speechRateMultiplier?: number): void {
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
   * @param node Current node being spoken.
   * @param currentWord Character offsets of
   *    current word spoken within node if word highlighting is enabled.
   */
  private updateHighlight_(
      node: AutomationNode,
      // TODO(b/314204374): Convert null to undefined.
      currentWord: {start: number, end: number}|null, paused: boolean): void {
    if (!currentWord) {
      chrome.accessibilityPrivate.setHighlights(
          [], this.prefsManager_.highlightColor());
      return;
    }
    // getStartCharIndexInParent is only defined for nodes with role
    // INLINE_TEXT_BOX.
    const charIndexInParent =
        node.role === chrome.automation.RoleType.INLINE_TEXT_BOX ?
        ParagraphUtils.getStartCharIndexInParent(node) :
        0;
    node.boundsForRange(
        currentWord.start - charIndexInParent,
        currentWord.end - charIndexInParent, bounds => {
          const highlights = bounds ? [bounds] : [];
          chrome.accessibilityPrivate.setHighlights(
              highlights, this.prefsManager_.highlightColor());
          if (!paused) {
            // If speech is ongoing, update the bounds. (If it was paused,
            // reading focus hasn't actually changed, so there's no need for
            // this notification).
            chrome.accessibilityPrivate.setSelectToSpeakFocus(
                bounds ? bounds : node.location);
          }
        });
  }

  /**
   * Renders user selection rect, in the form of a focus ring.
   */
  setSelectionRect(rect: chrome.accessibilityPrivate.ScreenRect): void {
    // TODO(crbug.com/40753028): Support showing two focus rings at once, in case
    // a focus ring highlighting a node group is already present.
    this.setFocusRings_(
        [rect], false /* don't draw background */, false /* panelVisible */);
  }

  /**
   * Updates overlay UI based on current node and panel state.
   * @param nodeGroup Current node group.
   * @param node Current node being spoken.
   * @param currentWord Character offsets of
   *    current word spoken within node if word highlighting is enabled.
   */
  update(
      nodeGroup: ParagraphUtils.NodeGroup, node: AutomationNode,
      // TODO(b/314204374): Convert null to undefined.
      currentWord: {start: number, end: number}|null,
      panelState:
          {showPanel: boolean, paused: boolean, speechRateMultiplier: number}):
      void {
    const {showPanel, paused, speechRateMultiplier} = panelState;
    // Show the block parent of the currently verbalized node with the
    // focus ring. If the node has no siblings in the group, highlight just
    // the one node.
    let focusRingRect;
    const currentBlockParent = nodeGroup.blockParent;
    if (currentBlockParent !== null && nodeGroup.nodes.length > 1) {
      focusRingRect = currentBlockParent!.location;
    } else {
      focusRingRect = node.location;
    }
    this.updateHighlight_(node, currentWord, paused);
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
  clear(): void {
    this.setFocusRings_(
        [], false /* do not draw background */, false /* panel not visible */);
    chrome.accessibilityPrivate.setHighlights(
        [], this.prefsManager_.highlightColor());
    this.updatePanel_(false /* hide panel */);
  }

  /**
   * @return Whether given node is the Select-to-speak floating panel.
   */
  static isPanel(node?: AutomationNode): boolean {
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
   * @return Whether given node is the Select-to-speak tray button.
   */
  static isTrayButton(node?: AutomationNode): boolean {
    if (!node) {
      return false;
    }
    return AutomationUtil.getAncestors(node).find(n => {
      return n.className === SELECT_TO_SPEAK_TRAY_CLASS_NAME;
    }) !== undefined;
  }
}

TestImportManager.exportForTesting(
    UiManager,
    ['SELECT_TO_SPEAK_TRAY_CLASS_NAME', SELECT_TO_SPEAK_TRAY_CLASS_NAME]);
