// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputHandler} from './input_handler.js';
import {MetricsUtils} from './metrics_utils.js';
import {NodeUtils} from './node_utils.js';
import {ParagraphUtils} from './paragraph_utils.js';
import {PrefsManager} from './prefs_manager.js';
import {SelectToSpeakConstants} from './select_to_speak_constants.js';
import {SentenceUtils} from './sentence_utils.js';
import {WordUtils} from './word_utils.js';

const AutomationNode = chrome.automation.AutomationNode;
const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const AccessibilityFeature = chrome.accessibilityPrivate.AccessibilityFeature;
const SelectToSpeakPanelAction =
    chrome.accessibilityPrivate.SelectToSpeakPanelAction;
const FocusRingStackingOrder =
    chrome.accessibilityPrivate.FocusRingStackingOrder;
const SelectToSpeakState = chrome.accessibilityPrivate.SelectToSpeakState;

// This must be the same as in ash/system/accessibility/select_to_speak_tray.cc:
// ash::kSelectToSpeakTrayClassName.
export const SELECT_TO_SPEAK_TRAY_CLASS_NAME =
    'tray/TrayBackgroundView/SelectToSpeakTray';

// This must match the name of view class that implements the menu view:
// ash/system/accessibility/select_to_speak_menu_view.h
const SELECT_TO_SPEAK_MENU_CLASS_NAME = 'SelectToSpeakMenuView';

// This must match the name of view class that implements the speed view:
// ash/system/accessibility/select_to_speak_speed_view.h
const SELECT_TO_SPEAK_SPEED_CLASS_NAME = 'SelectToSpeakSpeedView';

// This must match the name of view class that implements the bubble views:
// ash/system/tray/tray_bubble_view.h
const TRAY_BUBBLE_VIEW_CLASS_NAME = 'TrayBubbleView';

// This must match the name of view class that implements the buttons used in
// the floating panel:
// ash/system/accessibility/floating_menu_button.h
const FLOATING_MENU_BUTTON_CLASS_NAME = 'FloatingMenuButton';

// Matches one of the known GSuite apps which need the clipboard to find and
// read selected text. Includes sandbox and non-sandbox versions.
const GSUITE_APP_REGEXP =
    /^https:\/\/docs\.(?:sandbox\.)?google\.com\/(?:(?:presentation)|(?:document)|(?:spreadsheets)|(?:drawings)){1}\//;

// A RGBA hex string for the default background shading color, which is black at
// 40% opacity (hex 66). This should be equivalent to using
// AshColorProvider::ShieldLayerType kShield40.
const DEFAULT_BACKGROUND_SHADING_COLOR = '#0006';

// Settings key for system speech rate setting.
const SPEECH_RATE_KEY = 'settings.tts.speech_rate';

/**
 * Determines if a node is in one of the known Google GSuite apps that needs
 * special case treatment for speaking selected text. Not all Google GSuite
 * pages are included, because some are not known to have a problem with
 * selection: Forms is not included since it's relatively similar to any HTML
 * page, for example.
 * @param {AutomationNode=}  node The node to check
 * @return {?AutomationNode} The root node of the GSuite app, or null if none is
 *     found.
 */
export function getGSuiteAppRoot(node) {
  while (node !== undefined && node.root !== undefined) {
    if (node.root.url !== undefined && GSUITE_APP_REGEXP.exec(node.root.url)) {
      return node.root;
    }
    node = node.root.parent;
  }
  return null;
}

export class SelectToSpeak {
  constructor() {
    /**
     * The current state of the SelectToSpeak extension, from
     * SelectToSpeakState.
     * @private {!chrome.accessibilityPrivate.SelectToSpeakState}
     */
    this.state_ = SelectToSpeakState.INACTIVE;

    /**
     * Whether the TTS is on pause. When |this.state_| is
     * SelectToSpeakState.SPEAKING, |this.paused_| indicates whether we are
     * putting TTS on hold.
     * TODO(leileilei): use SelectToSpeakState.PAUSE to indicate the status.
     * @private {boolean}
     */
    this.ttsPaused_ = false;

    /**
     * Function to be called when STS finishes a pausing request.
     * @private {?function()}
     */
    this.pauseCompleteCallback_ = null;

    /** @type {InputHandler} */
    this.inputHandler_ = null;

    /** @private {chrome.automation.AutomationNode} */
    this.desktop_;

    /** @private {?chrome.automation.AutomationNode} */
    this.panel_ = null;

    /** @private {number|undefined} */
    this.intervalRef_;

    chrome.automation.getDesktop(function(desktop) {
      this.desktop_ = desktop;

      // After the user selects a region of the screen, we do a hit test at
      // the center of that box using the automation API. The result of the
      // hit test is a MOUSE_RELEASED accessibility event.
      desktop.addEventListener(
          EventType.MOUSE_RELEASED, this.onAutomationHitTest_.bind(this), true);

      // When Select-To-Speak is active, we do a hit test on the active node
      // and the result is a HOVER accessibility event. This event is used to
      // check that the current node is in the foreground window.
      desktop.addEventListener(
          EventType.HOVER, this.onHitTestCheckCurrentNodeMatches_.bind(this),
          true);

      // Listen to focus changes so we can grab the floating panel when it
      // goes into focus, so it can be used later without having to search
      // through the entire tree.
      desktop.addEventListener(
          EventType.FOCUS, this.onFocusChange_.bind(this), true);
    }.bind(this));

    /** @private {boolean} */
    this.readAfterClose_ = true;

    /**
     * The node groups to be spoken. We process content into node groups and
     * pass one node group at a time to the TTS engine. Note that we do not use
     * node groups for user-selected text in Gsuite. See readNodesInSelection_.
     * @private {!Array<!ParagraphUtils.NodeGroup>}
     */
    this.currentNodeGroups_ = [];

    /**
     * The index for the node group currently being spoken in
     * |this.currentNodeGroups_|.
     * @private {number}
     */
    this.currentNodeGroupIndex_ = -1;

    /**
     * The node group item currently being spoken. A node group item is a
     * representation of the original input nodes, but may not be the same. For
     * example, an input inline text node will be represented by its static text
     * node in the node group item.
     * @private {?ParagraphUtils.NodeGroupItem}
     */
    this.currentNodeGroupItem_ = null;

    /**
     * The index for the current node group item within the current node group,
     * The current node group can be accessed from |this.currentNodeGroups_|
     * using |this.currentNodeGroupIndex_|. In most cases,
     * |this.currentNodeGroupItemIndex_| can be used to get
     * |this.currentNodeGroupItem_| from the current node group. However, in
     * Gsuite, we will have node group items outside of a node group.
     * @private {number}
     */
    this.currentNodeGroupItemIndex_ = -1;

    /**
     * The indexes within the current node group item representing the word
     * currently being spoken. Only updated if word highlighting is enabled.
     * @private {?Object}
     */
    this.currentNodeWord_ = null;

    /**
     * The start char index of the word to be spoken. The index is relative
     * to the text content of the current node group.
     * @private {number}
     */
    this.currentCharIndex_ = -1;

    /**
     * Whether we are reading user-selected content. True if the current
     * content is from mouse or keyboard selection. False if the current
     * content is processed by the navigation features like paragraph
     * navigation, sentence navigation, pause and resume.
     * @private {boolean}
     */
    this.isUserSelectedContent_ = false;

    /**
     * Whether the current nodes support use of the navigation panel.
     * @private {boolean}
     */
    this.supportsNavigationPanel_ = true;

    /**
     * The position of the current focus ring, which usually highlights the
     * entire paragraph. Keep this as a member variable so that the control
     * panel can be updated easily.
     * @private {!Array<!chrome.accessibilityPrivate.ScreenRect>}
     */
    this.currentFocusRing_ = [];

    /** @private {boolean} */
    this.visible_ = true;

    /** @private {boolean} */
    this.scrollToSpokenNode_ = false;

    /**
     * The interval ID from a call to setInterval, which is set whenever
     * speech is in progress.
     * @private {number|undefined}
     */
    this.intervalId_;

    /** @private {Audio} */
    this.null_selection_tone_ =
        new Audio('select_to_speak/earcons/null_selection.ogg');

    /** @private {PrefsManager} */
    this.prefsManager_ = new PrefsManager();
    this.prefsManager_.initPreferences();

    this.runContentScripts_();
    this.setUpEventListeners_();

    /**
     * Function to be called when a state change request is received from the
     * accessibilityPrivate API.
     * @type {?function()}
     * @protected
     */
    this.onStateChangeRequestedCallbackForTest_ = null;

    /**
     * Feature flag controlling STS language detection integration.
     * @type {boolean}
     */
    this.enableLanguageDetectionIntegration_ = false;

    // TODO(chrishall): do we want to (also?) expose this in preferences?
    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-language-detection', (result) => {
          this.enableLanguageDetectionIntegration_ = result;
        });

    /**
     * Feature flag controlling STS navigation control.
     * @type {boolean}
     */
    this.navigationControlFlag_ = false;
    chrome.accessibilityPrivate.isFeatureEnabled(
        AccessibilityFeature.SELECT_TO_SPEAK_NAVIGATION_CONTROL, (result) => {
          this.navigationControlFlag_ = result;
        });

    /** @private {number} Default speech rate set in system settings. */
    this.systemSpeechRate_ = 1.0;
    chrome.settingsPrivate.getPref(SPEECH_RATE_KEY, (pref) => {
      if (!pref) {
        return;
      }
      this.systemSpeechRate_ = /** @type {number} */ (pref.value);
    });

    /** @private {?number} Speech rate that overrides system rate. */
    this.overrideSpeechRate_ = null;
  }

  /**
   * Gets the node group currently being spoken.
   * @return {!ParagraphUtils.NodeGroup|undefined}
   */
  getCurrentNodeGroup_() {
    return this.currentNodeGroups_[this.currentNodeGroupIndex_];
  }

  /**
   * Determines if navigation controls should be shown (and other related
   * functionality, such as auto-dismiss and click-to-navigate to sentence,
   * should be activated) based on feature flag and user setting.
   * @private
   */
  shouldShowNavigationControls_() {
    return this.navigationControlFlag_ &&
        this.prefsManager_.navigationControlsEnabled() &&
        this.supportsNavigationPanel_;
  }

  /**
   * Called in response to our hit test after the mouse is released,
   * when the user is in a mode where select-to-speak is capturing
   * mouse events (for example holding down Search).
   * @param {!AutomationEvent} evt The automation event.
   * @private
   */
  onAutomationHitTest_(evt) {
    // Walk up to the nearest window, web area, toolbar, or dialog that the
    // hit node is contained inside. Only speak objects within that
    // container. In the future we might include other container-like
    // roles here.
    var root = evt.target;
    // TODO: Use AutomationPredicate.root instead?
    while (root.parent && root.role !== RoleType.WINDOW &&
           root.role !== RoleType.ROOT_WEB_AREA &&
           root.role !== RoleType.DESKTOP && root.role !== RoleType.DIALOG &&
           root.role !== RoleType.ALERT_DIALOG &&
           root.role !== RoleType.TOOLBAR) {
      root = root.parent;
    }

    var rect = this.inputHandler_.getMouseRect();
    var nodes = [];
    chrome.automation.getFocus(function(focusedNode) {
      // In some cases, e.g. ARC++, the window received in the hit test request,
      // which is computed based on which window is the event handler for the
      // hit point, isn't the part of the tree that contains the actual
      // content. In such cases, use focus to get the root.
      // TODO(katie): Determine if this work-around needs to be ARC++ only. If
      // so, look for classname exoshell on the root or root parent to confirm
      // that a node is in ARC++.
      if (!NodeUtils.findAllMatching(root, rect, nodes) && focusedNode &&
          focusedNode.root.role !== RoleType.DESKTOP) {
        NodeUtils.findAllMatching(focusedNode.root, rect, nodes);
      }
      if (nodes.length === 1 &&
          AutomationUtil.getAncestors(nodes[0]).find(
              (n) => n.className === SELECT_TO_SPEAK_TRAY_CLASS_NAME)) {
        // Don't read only the Select-to-Speak toggle button in the tray unless
        // more items are being read.
        return;
      }
      if (this.shouldShowNavigationControls_() && nodes.length > 0 &&
          (rect.width <= SelectToSpeakConstants.PARAGRAPH_SELECTION_MAX_SIZE ||
           rect.height <=
               SelectToSpeakConstants.PARAGRAPH_SELECTION_MAX_SIZE)) {
        // If this is a single click (zero sized selection) on a text node, then
        // expand to entire paragraph.
        nodes = NodeUtils.getAllNodesInParagraph(nodes[0]);
      }
      this.startSpeechQueue_(
          nodes, {clearFocusRing: true, isUserSelectedContent: true});
      MetricsUtils.recordStartEvent(
          MetricsUtils.StartSpeechMethod.MOUSE, this.prefsManager_);
    }.bind(this));
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
      this.panel_ = windowParent;
    }
  }

  /**
   * Queues up selected text for reading by finding the Position objects
   * representing the selection.
   * @private
   */
  requestSpeakSelectedText_(focusedNode) {
    // If nothing is selected, return early.
    if (!focusedNode || !focusedNode.root ||
        !focusedNode.root.selectionStartObject ||
        !focusedNode.root.selectionEndObject) {
      this.onNullSelection_();
      return;
    }

    const startObject = focusedNode.root.selectionStartObject;
    const startOffset = focusedNode.root.selectionStartOffset || 0;
    const endObject = focusedNode.root.selectionEndObject;
    const endOffset = focusedNode.root.selectionEndOffset || 0;
    if (startObject === endObject && startOffset === endOffset) {
      this.onNullSelection_();
      return;
    }

    // First calculate the equivalent position for this selection.
    // Sometimes the automation selection returns an offset into a root
    // node rather than a child node, which may be a bug. This allows us to
    // work around that bug until it is fixed or redefined.
    // Note that this calculation is imperfect: it uses node name length
    // to index into child nodes. However, not all node names are
    // user-visible text, so this does not always work. Instead, we must
    // fix the Blink bug where focus offset is not specific enough to
    // say which node is selected and at what charOffset. See
    // https://crbug.com/803160 for more.

    const startPosition =
        NodeUtils.getDeepEquivalentForSelection(startObject, startOffset, true);
    const endPosition =
        NodeUtils.getDeepEquivalentForSelection(endObject, endOffset, false);

    // TODO(katie): We go into these blocks but they feel redundant. Can
    // there be another way to do this?
    let firstPosition;
    let lastPosition;
    if (startPosition.node === endPosition.node) {
      if (startPosition.offset < endPosition.offset) {
        firstPosition = startPosition;
        lastPosition = endPosition;
      } else {
        lastPosition = startPosition;
        firstPosition = endPosition;
      }
    } else {
      const dir =
          AutomationUtil.getDirection(startPosition.node, endPosition.node);
      // Highlighting may be forwards or backwards. Make sure we start at the
      // first node.
      if (dir === constants.Dir.FORWARD) {
        firstPosition = startPosition;
        lastPosition = endPosition;
      } else {
        lastPosition = startPosition;
        firstPosition = endPosition;
      }
    }

    this.cancelIfSpeaking_(true /* clear the focus ring */);
    this.readNodesInSelection_(firstPosition, lastPosition, focusedNode);
  }

  /**
   * Reads nodes between the first and last position selected by the user.
   * @param {NodeUtils.Position} firstPosition The first position at which to
   *     start reading.
   * @param {NodeUtils.Position} lastPosition The last position at which to
   *     stop reading.
   * @param {AutomationNode} focusedNode The node with user focus.
   * @private
   */
  readNodesInSelection_(firstPosition, lastPosition, focusedNode) {
    const nodes = [];
    let selectedNode = firstPosition.node;
    if (selectedNode.name && firstPosition.offset < selectedNode.name.length &&
        !NodeUtils.shouldIgnoreNode(
            selectedNode, /* include offscreen */ true) &&
        !NodeUtils.isNotSelectable(selectedNode)) {
      // Initialize to the first node in the list if it's valid and inside
      // of the offset bounds.
      nodes.push(selectedNode);
    } else {
      // The selectedNode actually has no content selected. Let the list
      // initialize itself to the next node in the loop below.
      // This can happen if you click-and-drag starting after the text in
      // a first line to highlight text in a second line.
      firstPosition.offset = 0;
    }
    while (selectedNode && selectedNode !== lastPosition.node &&
           AutomationUtil.getDirection(selectedNode, lastPosition.node) ===
               constants.Dir.FORWARD) {
      // TODO: Is there a way to optimize the directionality checking of
      // AutomationUtil.getDirection(selectedNode, finalNode)?
      // For example, by making a helper and storing partial computation?
      selectedNode = AutomationUtil.findNextNode(
          selectedNode, constants.Dir.FORWARD,
          AutomationPredicate.leafWithText);
      if (!selectedNode) {
        break;
      } else if (NodeUtils.isTextField(selectedNode)) {
        // Dive down into the next text node.
        // Why does leafWithText return text fields?
        selectedNode = AutomationUtil.findNextNode(
            selectedNode, constants.Dir.FORWARD,
            AutomationPredicate.leafWithText);
        if (!selectedNode) {
          break;
        }
      }
      if (!NodeUtils.shouldIgnoreNode(
              selectedNode, /* include offscreen */ true) &&
          !NodeUtils.isNotSelectable(selectedNode)) {
        nodes.push(selectedNode);
      }
    }
    if (nodes.length > 0) {
      if (lastPosition.node !== nodes[nodes.length - 1]) {
        // The node at the last position was not added to the list, perhaps it
        // was whitespace or invisible. Clear the ending offset because it
        // relates to a node that doesn't exist.
        this.startSpeechQueue_(nodes, {
          clearFocusRing: true,
          startCharIndex: firstPosition.offset,
          isUserSelectedContent: true
        });
      } else {
        this.startSpeechQueue_(nodes, {
          clearFocusRing: true,
          startCharIndex: firstPosition.offset,
          endCharIndex: lastPosition.offset,
          isUserSelectedContent: true
        });
      }
      this.initializeScrollingToOffscreenNodes_(focusedNode.root);
      MetricsUtils.recordStartEvent(
          MetricsUtils.StartSpeechMethod.KEYSTROKE, this.prefsManager_);
    } else {
      // Gsuite apps include webapps beyond Docs, see getGSuiteAppRoot and
      // GSUITE_APP_REGEXP.
      const gsuiteAppRootNode = getGSuiteAppRoot(focusedNode);
      if (!gsuiteAppRootNode) {
        return;
      }
      chrome.tabs.query({active: true}, (tabs) => {
        // Closure doesn't realize that we did a !gsuiteAppRootNode earlier
        // so we check again here.
        if (tabs.length === 0 || !gsuiteAppRootNode) {
          return;
        }
        const tab = tabs[0];
        this.inputHandler_.onRequestReadClipboardData();
        this.currentNodeGroupItem_ =
            new ParagraphUtils.NodeGroupItem(gsuiteAppRootNode, 0, false);
        chrome.tabs.executeScript(tab.id, {
          allFrames: true,
          matchAboutBlank: true,
          code: 'document.execCommand("copy");'
        });
        MetricsUtils.recordStartEvent(
            MetricsUtils.StartSpeechMethod.KEYSTROKE, this.prefsManager_);
      });
    }
  }

  /**
   * Gets ready to cancel future scrolling to offscreen nodes as soon as
   * a user-initiated scroll is done.
   * @param {AutomationNode=} root The root node to listen for events on.
   * @private
   */
  initializeScrollingToOffscreenNodes_(root) {
    if (!root) {
      return;
    }
    this.scrollToSpokenNode_ = true;
    const listener = (event) => {
      if (event.eventFrom !== 'action') {
        // User initiated event. Cancel all future scrolling to spoken nodes.
        // If the user wants a certain scroll position we will respect that.
        this.scrollToSpokenNode_ = false;

        // Now remove these event listeners, we no longer need them.
        root.removeEventListener(
            EventType.SCROLL_POSITION_CHANGED, listener, false);
        root.removeEventListener(
            EventType.SCROLL_HORIZONTAL_POSITION_CHANGED, listener, false);
        root.removeEventListener(
            EventType.SCROLL_VERTICAL_POSITION_CHANGED, listener, false);
      }
    };
    // ARC++ fires the first event, Views/Web fire the horizontal/vertical
    // scroll position changed events via AXEventGenerator.
    root.addEventListener(EventType.SCROLL_POSITION_CHANGED, listener, false);
    root.addEventListener(
        EventType.SCROLL_HORIZONTAL_POSITION_CHANGED, listener, false);
    root.addEventListener(
        EventType.SCROLL_VERTICAL_POSITION_CHANGED, listener, false);
  }

  /**
   * Plays a tone to let the user know they did the correct
   * keystroke but nothing was selected.
   * @private
   */
  onNullSelection_() {
    if (!this.shouldShowNavigationControls_()) {
      this.null_selection_tone_.play();
      return;
    }

    this.focusPanel_();
  }

  /**
   * Sets focus to the floating control panel, if present.
   * @private
   */
  focusPanel_() {
    // Used cached panel node if possible to avoid expensive desktop.find().
    // Note: Checking role attribute to see if node is still valid.
    if (this.panel_ && this.panel_.role) {
      this.panel_.focus();
      return;
    }
    this.panel_ = null;

    // Fallback to more expensive method of finding panel.
    const menuView = this.desktop_.find(
        {attributes: {className: SELECT_TO_SPEAK_MENU_CLASS_NAME}});
    if (menuView !== null && menuView.parent &&
        menuView.parent.className === TRAY_BUBBLE_VIEW_CLASS_NAME) {
      // The menu view's parent is the TrayBubbleView can can be assigned focus.
      this.panel_ = menuView.parent;
      this.panel_.focus();
    }
  }

  /**
   * Whether the STS is on a pause state, where |this.ttsPaused_| is true and
   * |this.state_| is SPEAKING.
   * @private
   * TODO(leileilei): use SelectToSpeakState.PAUSE to indicate the status.
   */
  isPaused_() {
    return this.ttsPaused_ && this.state_ === SelectToSpeakState.SPEAKING;
  }

  /**
   * Set |this.ttsPaused_| and |this.state_| according to pause status.
   * @param {boolean} shouldPause whether the TTS is on pause or speaking.
   * @private
   * TODO(leileilei): use SelectToSpeakState.PAUSE to indicate the status and
   * consider refactoring the name of this function.
   */
  updatePauseStatusFromTtsEvent_(shouldPause) {
    this.ttsPaused_ = shouldPause;
    this.onStateChanged_(SelectToSpeakState.SPEAKING);
    if (shouldPause && this.pauseCompleteCallback_) {
      this.pauseCompleteCallback_();
    }
  }

  /**
   * Pause the TTS. We do not assert isPaused_() before stopping TTS in case
   * |this.ttsPaused_| was true while tts is speaking. This function also sets
   * the |this.pauseCompleteCallback_|, which will be executed at the end of
   * the pause process in |updatePauseStatusFromTtsEvent_|. This enables us to
   * execute functions when the pause request is finished. For example, to
   * navigate the next sentence, we trigger pause_ and start finding the next
   * sentence when the pause function is fulfilled.
   * @return {!Promise}
   * @private
   */
  pause_() {
    return new Promise((resolve) => {
      this.pauseCompleteCallback_ = () => {
        this.pauseCompleteCallback_ = null;
        resolve();
      };
      chrome.tts.stop();
      // If the user triggers pause_() or navigation features that use pause_()
      // (e.g., sentence navigation), the following reading content will not be
      // user-selected content. This enables us to distinguish between a user-
      // trigger pause from the auto pause happening at the end of user-selected
      // content.
      this.isUserSelectedContent_ = false;
    });
  }

  /**
   * Resume the TTS. If there is no remaining content in this paragraph, we will
   * navigate to the next paragraph. If there is still content in this
   * paragraph, STS behaves differently depending on the resume status. If we
   * resume from a user-trigger pause, we will resume from the start of the
   * current sentence. If we resume from the end of user-selected content, we
   * will continue reading remaining content.
   * @private
   */
  resume_() {
    // If TTS is not paused, return early.
    if (!this.isPaused_()) {
      return;
    }
    const currentNodeGroup = this.getCurrentNodeGroup_();

    // If there is no processed node group, that means the user has not selected
    // anything. Ignore the resume command.
    if (!currentNodeGroup) {
      return;
    }
    const {nodes: remainingNodes, offset} =
        NodeUtils.getNextNodesInParagraphFromNodeGroup(
            currentNodeGroup, this.currentCharIndex_, constants.Dir.FORWARD);
    // There is no remaining nodes in this paragraph so we navigate to the next
    // paragraph.
    if (remainingNodes.length === 0) {
      this.navigateToNextParagraph_(constants.Dir.FORWARD);
      return;
    }

    if (this.isUserSelectedContent_ ||
        SentenceUtils.isSentenceStart(
            currentNodeGroup, this.currentCharIndex_)) {
      // If we are resuming from the end of user-selected content or if we are
      // at the start of the current sentence, we should start reading the
      // remaining content.
      this.startSpeechQueue_(
          remainingNodes, {clearFocusRing: false, startCharIndex: offset});
      return;
    }

    // If the current position is not a sentence start, navigate to the start of
    // this sentence.
    this.navigateToNextSentence_(constants.Dir.BACKWARD);
  }

  /**
   * Stop speech. If speech was in-progress, the interruption
   * event will be caught and clearFocusRingAndNode_ will be
   * called, stopping visual feedback as well.
   * If speech was not in progress, i.e. if the user was drawing
   * a focus ring on the screen, this still clears the visual
   * focus ring.
   * @private
   */
  stopAll_() {
    chrome.tts.stop();
    this.clearFocusRing_();
    this.overrideSpeechRate_ = null;  // Reset speech rate to system default
    this.onStateChanged_(SelectToSpeakState.INACTIVE);
  }

  /**
   * Clears the current focus ring and node, but does
   * not stop the speech.
   * @private
   */
  clearFocusRingAndNode_() {
    this.clearFocusRing_();
    // Clear the node and also stop the interval testing.
    this.resetNodes_();
    this.supportsNavigationPanel_ = true;
    this.isUserSelectedContent_ = false;
    clearInterval(this.intervalId_);
    this.intervalId_ = undefined;
    this.scrollToSpokenNode_ = false;
  }

  /**
   * Resets the instance variables for nodes and node groups.
   * @private
   */
  resetNodes_() {
    this.currentNodeGroups_ = [];
    this.currentNodeGroupIndex_ = -1;
    this.currentNodeGroupItem_ = null;
    this.currentNodeGroupItemIndex_ = -1;
    this.currentNodeWord_ = null;
    this.currentCharIndex_ = -1;
  }

  /**
   * Update the navigation floating panel.
   * @private
   */
  updateNavigationPanel_() {
    if (this.shouldShowNavigationControls_() && this.currentFocusRing_.length) {
      // If the feature is enabled and we have a valid focus ring, flip the
      // pause and resume button according to the current STS and TTS state.
      // Also, update the location of the panel according to the focus ring.
      chrome.accessibilityPrivate.updateSelectToSpeakPanel(
          /* show= */ true, /* anchor= */ this.currentFocusRing_[0],
          /* isPaused= */ this.isPaused_(),
          /* speed= */ this.getSpeechRate_());
    } else {
      // Dismiss the panel if either the feature is disabled or the focus ring
      // is not valid.
      chrome.accessibilityPrivate.updateSelectToSpeakPanel(/* show= */ false);
    }
  }

  /**
   * Clears the focus ring, but does not clear the current
   * node.
   * @private
   */
  clearFocusRing_() {
    this.setFocusRings_([], false /* do not draw background */);
    chrome.accessibilityPrivate.setHighlights(
        [], this.prefsManager_.highlightColor());
    this.updateNavigationPanel_();
  }

  /**
   * Sets the focus ring to |rects|. If |drawBackground|, draws the grey focus
   * background with the alpha set in prefs.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @param {boolean} drawBackground
   * @private
   */
  setFocusRings_(rects, drawBackground) {
    this.currentFocusRing_ = rects;
    let color = '#0000';  // Fully transparent.
    if (drawBackground && this.prefsManager_.backgroundShadingEnabled()) {
      color = DEFAULT_BACKGROUND_SHADING_COLOR;
    }
    // If we're also showing a navigation panel, ensure the focus ring appears
    // below the panel UI.
    const stackingOrder = this.shouldShowNavigationControls_() ?
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
   * Runs content scripts that allow Select-to-Speak access to
   * Google Docs content without a11y mode enabled, in every open
   * tab. Should be run when Select-to-Speak starts up so that any
   * tabs already opened will be checked.
   * This should be kept in sync with the "content_scripts" section in
   * the Select-to-Speak manifest.
   * @private
   */
  runContentScripts_() {
    const scripts = chrome.runtime.getManifest()['content_scripts'][0]['js'];

    // We only ever expect one content script.
    if (scripts.length !== 1) {
      throw new Error(
          'Only expected one script; got ' + JSON.stringify(scripts));
    }

    const script = scripts[0];

    chrome.tabs.query(
        {
          url: [
            'https://docs.google.com/document*',
            'https://docs.sandbox.google.com/*'
          ]
        },
        (tabs) => {
          tabs.forEach((tab) => {
            chrome.tabs.executeScript(tab.id, {file: script});
          });
        });
  }

  /**
   * Set up event listeners user input.
   * @private
   */
  setUpEventListeners_() {
    this.inputHandler_ = new InputHandler({
      // canStartSelecting: Whether mouse selection can begin.
      canStartSelecting: () => {
        return this.state_ !== SelectToSpeakState.SELECTING;
      },
      // onSelectingStateChanged: Started or stopped mouse selection.
      onSelectingStateChanged: (isSelecting, x, y) => {
        if (isSelecting) {
          this.onStateChanged_(SelectToSpeakState.SELECTING);
          // Fire a hit test event on click to warm up the cache, and cancel
          // if speaking.
          this.cancelIfSpeaking_(false /* don't clear the focus ring */);
          this.desktop_.hitTest(x, y, EventType.MOUSE_PRESSED);
        } else {
          this.onStateChanged_(SelectToSpeakState.INACTIVE);
          // Do a hit test at the center of the area the user dragged over.
          // This will give us some context when searching the accessibility
          // tree. The hit test will result in a EventType.MOUSE_RELEASED
          // event being fired on the result of that hit test, which will
          // trigger onAutomationHitTest_.
          this.desktop_.hitTest(x, y, EventType.MOUSE_RELEASED);
        }
      },
      // onSelectionChanged: Mouse selection rect changed.
      onSelectionChanged: rect => {
        this.setFocusRings_([rect], false /* don't draw background */);
      },
      // onKeystrokeSelection: Keys pressed for reading highlighted text.
      onKeystrokeSelection: () => {
        chrome.automation.getFocus(this.requestSpeakSelectedText_.bind(this));
      },
      // onRequestCancel: User requested canceling input/speech.
      onRequestCancel: () => {
        // User manually requested cancel, so log cancel metric.
        MetricsUtils.recordCancelIfSpeaking();
        this.cancelIfSpeaking_(true /* clear the focus ring */);
      },
      // onTextReceived: Text received from a 'paste' event to read aloud.
      onTextReceived: this.startSpeech_.bind(this)
    });
    this.inputHandler_.setUpEventListeners();
    chrome.accessibilityPrivate.onSelectToSpeakStateChangeRequested.addListener(
        this.onStateChangeRequested_.bind(this));
    chrome.accessibilityPrivate.onSelectToSpeakPanelAction.addListener(
        this.onSelectToSpeakPanelAction_.bind(this));
    chrome.settingsPrivate.onPrefsChanged.addListener(
        this.onPrefsChanged_.bind(this));
    // Initialize the state to SelectToSpeakState.INACTIVE.
    chrome.accessibilityPrivate.setSelectToSpeakState(this.state_);
  }

  /**
   * Called when Chrome OS is requesting Select-to-Speak to switch states.
   */
  onStateChangeRequested_() {
    // Switch Select-to-Speak states on request.
    // We will need to track the current state and toggle from one state to
    // the next when this function is called, and then call
    // accessibilityPrivate.setSelectToSpeakState with the new state.
    switch (this.state_) {
      case SelectToSpeakState.INACTIVE:
        // Start selection.
        this.inputHandler_.setTrackingMouse(true);
        this.onStateChanged_(SelectToSpeakState.SELECTING);
        MetricsUtils.recordSelectToSpeakStateChangeEvent(
            MetricsUtils.StateChangeEvent.START_SELECTION);
        break;
      case SelectToSpeakState.SPEAKING:
        // Stop speaking. User manually requested, so log cancel metric.
        MetricsUtils.recordCancelIfSpeaking();
        this.cancelIfSpeaking_(true /* clear the focus ring */);
        MetricsUtils.recordSelectToSpeakStateChangeEvent(
            MetricsUtils.StateChangeEvent.CANCEL_SPEECH);
        break;
      case SelectToSpeakState.SELECTING:
        // Cancelled selection.
        this.inputHandler_.setTrackingMouse(false);
        this.onStateChanged_(SelectToSpeakState.INACTIVE);
        MetricsUtils.recordSelectToSpeakStateChangeEvent(
            MetricsUtils.StateChangeEvent.CANCEL_SELECTION);
    }
    this.onStateChangeRequestedCallbackForTest_ &&
        this.onStateChangeRequestedCallbackForTest_();
  }

  /**
   * Handles Select-to-speak panel action.
   * @param {!SelectToSpeakPanelAction} panelAction Action to perform.
   * @param {number=} value Optional value associated with action.
   * @private
   */
  onSelectToSpeakPanelAction_(panelAction, value) {
    if (!this.shouldShowNavigationControls_()) {
      // Ignore if this feature is not enabled.
      return;
    }
    switch (panelAction) {
      case SelectToSpeakPanelAction.NEXT_PARAGRAPH:
        this.navigateToNextParagraph_(constants.Dir.FORWARD);
        break;
      case SelectToSpeakPanelAction.PREVIOUS_PARAGRAPH:
        this.navigateToNextParagraph_(constants.Dir.BACKWARD);
        break;
      case SelectToSpeakPanelAction.NEXT_SENTENCE:
        this.navigateToNextSentence_(constants.Dir.FORWARD);
        break;
      case SelectToSpeakPanelAction.PREVIOUS_SENTENCE:
        this.navigateToNextSentence_(
            constants.Dir.BACKWARD, true /* skipCurrentSentence */);
        break;
      case SelectToSpeakPanelAction.EXIT:
        // User manually requested, so log cancel metric.
        MetricsUtils.recordCancelIfSpeaking();
        this.stopAll_();
        break;
      case SelectToSpeakPanelAction.PAUSE:
        MetricsUtils.recordPauseEvent();
        this.pause_();
        break;
      case SelectToSpeakPanelAction.RESUME:
        if (this.isPaused_()) {
          MetricsUtils.recordResumeEvent();
          this.resume_();
        }
        break;
      case SelectToSpeakPanelAction.CHANGE_SPEED:
        if (!value) {
          console.warn(
              'Change speed request receieved with invalid value', value);
          return;
        }
        this.changeSpeed_(value);
        break;
      default:
        // TODO(crbug.com/1140216): Implement other actions.
    }
  }

  /**
   * Handles system preferences change.
   * @param {!Array<!Object>} prefs
   * @private
   */
  onPrefsChanged_(prefs) {
    const ratePref = prefs.find((pref) => pref.key === SPEECH_RATE_KEY);
    if (ratePref) {
      this.systemSpeechRate_ = ratePref.value;
    }
  }

  /**
   * Navigates to the next sentence. First, we search the next sentence in the
   * current node group. If we do not find one, we will search within the
   * remaining content in the current paragraph (i.e., text block). If this
   * still fails, we will search the next paragraph.
   * TODO(leileilei@google.com): Handle the edge case where the user navigates
   * to next sentence from the end of a document, see http://crbug.com/1160962.
   * @param {constants.Dir} direction Direction to search for the next sentence.
   *     If set to forward, we look for the sentence start after the current
   *     position. Otherwise, we look for the sentence start before the current
   *     position.
   * @param {boolean} skipCurrentSentence Whether to skip the current sentence.
   *     This only affects backward navigation. When set to false, navigating
   *     backward will find the closest sentence start. When set to true,
   *     navigating backward will ignore the sentence start in the current
   *     sentence. For example, when navigating backward from the middle of a
   *     sentence. A true |skipCurrentSentence| will take us to the start of the
   *     previous sentence while a false one will take us to the start of the
   *     current sentence. Regardless of this parameter, navigating backward
   *     from a sentence start will take us to the start of the previous
   *     sentence.
   * @private
   */
  async navigateToNextSentence_(direction, skipCurrentSentence = false) {
    const currentNodeGroup = this.getCurrentNodeGroup_();

    // An empty node group is not expected and means that the user has not
    // enqueued any text.
    if (!currentNodeGroup) {
      return;
    }

    if (!this.isPaused_()) {
      await this.pause_();
    }

    // Checks the next sentence within this node group. If we have enqueued the
    // next sentence that fulfilled the requirements, return.
    if (this.enqueueNextSentenceWithinNodeGroup_(
            currentNodeGroup, this.currentCharIndex_, direction,
            skipCurrentSentence)) {
      return;
    }

    // If there is no next sentence at the current node group, look for the
    // content within this paragraph. First, we get the remaining content in
    // the paragraph. The returned offset marks the char index of the current
    // position in the paragraph. When searching forward, the offset is the
    // char index pointing to the beginning of the remaining content. When
    // searching backward, the offset is the char index pointing to the char
    // after the remaining content.
    const {nodes, offset} = NodeUtils.getNextNodesInParagraphFromNodeGroup(
        currentNodeGroup, this.currentCharIndex_, direction);
    // If we have reached to the end of a paragraph, enqueue the sentence from
    // the next paragraph.
    if (nodes.length === 0) {
      this.enqueueNextSentenceInNextParagraph_(direction);
      return;
    }
    // Get the node group for the remaining content in the paragraph. If we are
    // looking for the content after the current position, set startIndex as
    // offset. Otherwise, set endIndex as offset.
    const startIndex = direction === constants.Dir.FORWARD ? offset : undefined;
    const endIndex = direction === constants.Dir.FORWARD ? undefined : offset;
    const {nodeGroup, startIndexInGroup, endIndexInGroup} =
        ParagraphUtils.buildSingleNodeGroupWithOffset(
            nodes, startIndex, endIndex);
    // Search in the remaining content.
    const charIndex = direction === constants.Dir.FORWARD ? startIndexInGroup :
                                                            endIndexInGroup;
    // The charIndex is guaranteed to be valid at this point, although the
    // closure compiler is not able to detect it as a valid number.
    if (charIndex === undefined) {
      console.warn('Navigate sentence with an invalid char index', charIndex);
      return;
    }
    // When searching backward, we need to adjust |skipCurrentSentence| if it
    // is true. The remaining content we get excludes the char at
    // |this.currentCharIndex_|. If this char is a sentence
    // start, we have already skipped the current sentence so we need to change
    // |skipCurrentSentence| to false for the next search.
    if (direction === constants.Dir.BACKWARD && skipCurrentSentence) {
      const currentPositionIsSentenceStart = SentenceUtils.isSentenceStart(
          currentNodeGroup, this.currentCharIndex_);
      if (currentPositionIsSentenceStart) {
        skipCurrentSentence = false;
      }
    }
    if (this.enqueueNextSentenceWithinNodeGroup_(
            nodeGroup, charIndex, direction, skipCurrentSentence)) {
      return;
    }

    // If there is no next sentence within this paragraph, enqueue the sentence
    // from the next paragraph.
    this.enqueueNextSentenceInNextParagraph_(direction);
  }

  /**
   * Enqueues the next sentence within the |nodeGroup|. If the |direction|
   * is set to forward, it will navigate to the sentence start after the
   * |startCharIndex|. Otherwise, it will look for the sentence start before the
   * |startCharIndex|.
   * @param {ParagraphUtils.NodeGroup} nodeGroup
   * @param {number} startCharIndex The char index that we start from. This
   *     index is relative to the text content of this node group and is
   *     exclusive: if a sentence start at 0 and we search with a 0
   *     |startCharIndex|, this function will return the next sentence start
   *     after 0 if we search forward.
   * @param {constants.Dir} direction
   * @param {boolean} skipCurrentSentence Whether to skip the current sentence
   *     when navigating backward. See navigateToNextSentence_.
   * @return {boolean} Whether we have enqueued content to the speech queue.
   *     When |skipCurrentSentence| is true, we will not enqueue content to
   *     speech queue if we only find a sentence start in the current sentence.
   * @private
   */
  enqueueNextSentenceWithinNodeGroup_(
      nodeGroup, startCharIndex, direction, skipCurrentSentence) {
    if (!nodeGroup) {
      return false;
    }
    let nextSentenceStart =
        SentenceUtils.getSentenceStart(nodeGroup, startCharIndex, direction);
    if (nextSentenceStart === null) {
      return false;
    }
    // When we search backward, if we want to skip the current sentence, we
    // need to search the sentence start in the previous sentence. If the
    // position of |startCharIndex| is a sentence start, the current
    // |nextSentenceStart| is already in the previous sentence because
    // getSentenceStart excludes the search index. Otherwise, the
    // |nextSentenceStart| we found is the start of current sentence, and we
    // need to search backward again.
    if (direction === constants.Dir.BACKWARD && skipCurrentSentence &&
        !SentenceUtils.isSentenceStart(nodeGroup, startCharIndex)) {
      nextSentenceStart = SentenceUtils.getSentenceStart(
          nodeGroup, nextSentenceStart, direction);
    }
    // If the second sentence start is not valid, we do not enqueue text,
    if (nextSentenceStart === null) {
      return false;
    }

    // Get the content between the sentence start and the end of the paragraph.
    const {nodes, offset} = NodeUtils.getNextNodesInParagraphFromNodeGroup(
        nodeGroup, nextSentenceStart, constants.Dir.FORWARD);
    if (nodes.length === 0) {
      // There is no remaining content. Move to the next paragraph. This is
      // unexpected since we already found a sentence start, which indicates
      // there should be some content to read.
      this.enqueueNextSentenceInNextParagraph_(direction);
    } else {
      this.startSpeechQueue_(
          nodes, {clearFocusRing: false, startCharIndex: offset});
    }
    return true;
  }

  /**
   * Enqueues the next sentence in the next text block in the given
   * direction. If the |direction| is set to forward, it will navigate to the
   * start of the following text block. Otherwise, it will look for the last
   * sentence in the previous text block. This function will enqueue content to
   * the speech queue regardless of whether we have found a sentence start in
   * the text block.
   * @param {constants.Dir} direction
   * @private
   */
  enqueueNextSentenceInNextParagraph_(direction) {
    const paragraphNodes = this.locateNodesForNextParagraph_(direction);
    if (paragraphNodes.length === 0) {
      return;
    }
    // Ensure the first node in the paragraph is visible.
    paragraphNodes[0].makeVisible();

    if (direction === constants.Dir.FORWARD) {
      // If we are looking for the sentence start in the following text block,
      // start reading the nodes.
      this.startSpeechQueue_(paragraphNodes);
      return;
    }

    // If we are looking for the previous sentence start, search the last
    // sentence in the previous text block. Get the node group for the previous
    // text block. The returned startIndexInGroup and endIndexInGroup are
    // unused.
    const {nodeGroup, startIndexInGroup, endIndexInGroup} =
        ParagraphUtils.buildSingleNodeGroupWithOffset(paragraphNodes);
    // We search backward for the sentence start before the end of the text
    // block.
    const searchOffset = nodeGroup.text.length;
    const sentenceStartIndex = SentenceUtils.getSentenceStart(
        nodeGroup, searchOffset, constants.Dir.BACKWARD);
    // If there is no sentence start in the previous text block, start reading
    // the block.
    if (sentenceStartIndex === null) {
      this.startSpeechQueue_(paragraphNodes);
      return;
    }
    // Gets the remaining content between the sentence start until the end of
    // the text block. The offset is the start char index for the first node in
    // the remaining content.
    const {nodes, offset} = NodeUtils.getNextNodesInParagraphFromNodeGroup(
        nodeGroup, sentenceStartIndex, constants.Dir.FORWARD);
    if (nodes.length === 0) {
      // If there is no remaining content, start reading the block. This is
      // unexpected since we already found a sentence start, which indicates
      // there should be some content to read.
      this.startSpeechQueue_(paragraphNodes);
      return;
    }
    // Reads the remaining content from the sentence start until the end of the
    // block.
    this.startSpeechQueue_(
        nodes, {clearFocusRing: false, startCharIndex: offset});
  }

  /**
   * Navigates to the next text block in the given direction.
   * @param {constants.Dir} direction
   * @private
   */
  async navigateToNextParagraph_(direction) {
    if (!this.isPaused_()) {
      // Stop TTS if it is currently playing.
      await this.pause_();
    }

    const nodes = this.locateNodesForNextParagraph_(direction);
    if (nodes.length === 0) {
      return;
    }
    // Ensure the first node in the paragraph is visible.
    nodes[0].makeVisible();

    this.startSpeechQueue_(nodes);
  }

  /**
   * Finds the nodes for the next text block in the given direction. This
   * function is based on |NodeUtils.getNextParagraph| but provides additional
   * checks on the anchor node used for searchiong.
   * @param {constants.Dir} direction
   * @return {Array<!AutomationNode>} A list of nodes for the next block in the
   *     given direction.
   * @private
   */
  locateNodesForNextParagraph_(direction) {
    // Use current block parent as starting point to navigate from. If it is not
    // a valid block, then use one of the nodes that are currently activated.
    const currentNodeGroup = this.getCurrentNodeGroup_();
    if (!currentNodeGroup) {
      return [];
    }
    let node = currentNodeGroup.blockParent;
    if ((node === null || node.isRootNode || node.role === undefined) &&
        currentNodeGroup.nodes.length > 0) {
      node = currentNodeGroup.nodes[0].node;
    }
    if (node === null || node.role === undefined) {
      // Could not find any nodes to navigate from.
      return [];
    }

    // Retrieve the nodes that make up the next/prev paragraph.
    const nextParagraphNodes = NodeUtils.getNextParagraph(node, direction);
    if (nextParagraphNodes.length === 0) {
      // Cannot find any valid nodes in given direction.
      return [];
    }
    if (AutomationUtil.getAncestors(nextParagraphNodes[0])
            .find((n) => this.isPanel_(n))) {
      // Do not navigate to Select-to-speak panel.
      return [];
    }

    return nextParagraphNodes;
  }

  /**
   * Updates current reading speed (speech rate).
   * @param {number} rate
   * @private
   */
  async changeSpeed_(rate) {
    this.overrideSpeechRate_ = rate === this.systemSpeechRate_ ? null : rate;

    // If currently playing, stop TTS, then resume from current spot.
    if (!this.isPaused_()) {
      await this.pause_();
      this.resume_();
    }
  }

  /**
   * Enqueue speech for the single given string. The string is not associated
   * with any particular nodes, so this does not do any work around drawing
   * focus rings, unlike startSpeechQueue_ below.
   * @param {string} text The text to speak.
   * @private
   */
  startSpeech_(text) {
    this.prepareForSpeech_(true /* clearFocusRing */);
    const options = this.prefsManager_.speechOptions();
    options.onEvent = (event) => {
      if (event.type === 'start') {
        this.onStateChanged_(SelectToSpeakState.SPEAKING);
        this.testCurrentNode_();
      } else if (
          (event.type === 'end' || event.type === 'interrupted' ||
           event.type === 'cancelled') &&
          !this.shouldShowNavigationControls_()) {
        // Automatically dismiss when we're at the end, unless navigation
        // controled is enabled, in which case we persist STS.
        this.onStateChanged_(SelectToSpeakState.INACTIVE);
      }
    };
    chrome.tts.speak(text, options);
  }

  /**
   * Enqueue nodes to TTS queue and start TTS. This function can be used for
   * adding nodes, either from user selection (e.g., mouse selection) or
   * navigation control (e.g., next paragraph).
   * @param {!Array<AutomationNode>} nodes The nodes to speak.
   * @param {!{clearFocusRing: (boolean|undefined),
   *          startCharIndex: (number|undefined),
   *          endCharIndex: (number|undefined),
   *          isUserSelectedContent: (boolean|undefined)}=} opt_params
   *    clearFocusRing: Whether to clear the focus ring or not. For example, we
   * need to clear the focus ring when starting from scratch but we do not need
   * to clear the focus ring when resuming from a previous pause. If this is not
   * passed, will default to false.
   *    startCharIndex: The index into the first node's text at which to start
   * speaking. If this is not passed, will start at 0.
   *    endCharIndex: The index into the last node's text at which to end
   * speech. If this is not passed, will stop at the end.
   *    isUserSelectedContent: Whether the content is from user selection. If
   * this is not passed, will default to false.
   * @private
   */
  startSpeechQueue_(nodes, opt_params) {
    const params = opt_params || {};
    const clearFocusRing = params.clearFocusRing || false;
    let startCharIndex = params.startCharIndex;
    let endCharIndex = params.endCharIndex;
    this.isUserSelectedContent_ = params.isUserSelectedContent || false;

    this.prepareForSpeech_(clearFocusRing /* clear the focus ring */);

    if (nodes.length === 0) {
      return;
    }

    // Remember the original first and last node in the given list, as
    // |startCharIndex| and |endCharIndex| pertain to them. If, after SVG
    // resorting, the first or last nodes are re-ordered, do not clip them.
    const originalFirstNode = nodes[0];
    const originalLastNode = nodes[nodes.length - 1];
    // Sort any SVG child nodes, if present, by visual reading order.
    NodeUtils.sortSvgNodesByReadingOrder(nodes);
    // Override start or end index if original nodes were sorted.
    if (originalFirstNode !== nodes[0]) {
      startCharIndex = undefined;
    }
    if (originalLastNode !== nodes[nodes.length - 1]) {
      endCharIndex = undefined;
    }

    this.supportsNavigationPanel_ = this.isNavigationPanelSupported_(nodes);
    this.updateNodeGroups_(nodes, startCharIndex, endCharIndex);

    // Play TTS according to the current state variables.
    this.startCurrentNodeGroup_();
  }

  /**
   * Updates the node groups to be spoken. Converts |nodes|, |startCharIndex|,
   * and |endCharIndex| into node groups, and updates |this.currentNodeGroups_|
   * and |this.currentNodeGroupIndex_|.
   * @param {!Array<AutomationNode>} nodes The nodes to speak.
   * @param {number=} startCharIndex The index into the first node's text at
   *     which to start speaking. If this is not passed, will start at 0.
   * @param {number=} endCharIndex The index into the last node's text at which
   *     to end speech. If this is not passed, will stop at the end.
   * @private
   */
  updateNodeGroups_(nodes, startCharIndex, endCharIndex) {
    this.resetNodes_();

    for (let i = 0; i < nodes.length; i++) {
      // When navigation controls are enabled, disable the clipping of overflow
      // words. When overflow words are clipped, words scrolled out of view are
      // clipped, which is undesirable for our navigation features as we
      // generate node groups for next/previous paragraphs which may be fully or
      // partially scrolled out of view.
      const nodeGroup = ParagraphUtils.buildNodeGroup(nodes, i, {
        splitOnLanguage: this.enableLanguageDetectionIntegration_,
        clipOverflowWords: !this.shouldShowNavigationControls_(),
      });

      const isFirstNodeGroup = i === 0;
      const shouldApplyStartOffset =
          isFirstNodeGroup && startCharIndex !== undefined;
      const firstNodeHasInlineText =
          nodeGroup.nodes.length > 0 && nodeGroup.nodes[0].hasInlineText;
      if (shouldApplyStartOffset && firstNodeHasInlineText) {
        // We assume that the start offset will only be applied to the first
        // node in the first NodeGroup. The |startCharIndex| needs to be
        // adjusted. The first node of the NodeGroup may not be at the beginning
        // of the parent of the NodeGroup. (e.g., an inlineText in its
        // staticText parent). Thus, we need to adjust the start index.
        const startIndexInNodeParent =
            ParagraphUtils.getStartCharIndexInParent(nodes[0]);
        const startIndexInNodeGroup = startCharIndex + startIndexInNodeParent +
            nodeGroup.nodes[0].startChar;
        this.applyOffset(
            nodeGroup, startIndexInNodeGroup, true /* isStartOffset */);
      }

      // Advance i to the end of this group, to skip all nodes it contains.
      i = nodeGroup.endIndex;
      const isLastNodeGroup = (i === nodes.length - 1);
      const shouldApplyEndOffset =
          isLastNodeGroup && endCharIndex !== undefined;
      const lastNodeHasInlineText = nodeGroup.nodes.length > 0 &&
          nodeGroup.nodes[nodeGroup.nodes.length - 1].hasInlineText;
      if (shouldApplyEndOffset && lastNodeHasInlineText) {
        // We assume that the end offset will only be applied to the last node
        // in the last NodeGroup. Similarly, |endCharIndex| needs to be
        // adjusted.
        const startIndexInNodeParent =
            ParagraphUtils.getStartCharIndexInParent(nodes[i]);
        const endIndexInNodeGroup = endCharIndex + startIndexInNodeParent +
            nodeGroup.nodes[nodeGroup.nodes.length - 1].startChar;
        this.applyOffset(
            nodeGroup, endIndexInNodeGroup, false /* isStartOffset */);
      }
      if (nodeGroup.nodes.length === 0 && !isLastNodeGroup) {
        continue;
      }
      this.currentNodeGroups_.push(nodeGroup);
    }
    // Sets the initial node group index to zero if this.currentNodeGroups_ has
    // items.
    if (this.currentNodeGroups_.length > 0) {
      this.currentNodeGroupIndex_ = 0;
    }
  }

  /**
   * Starts reading the current node group.
   * @private
   */
  startCurrentNodeGroup_() {
    const nodeGroup = this.getCurrentNodeGroup_();
    if (!nodeGroup) {
      return;
    }
    const options = {};
    // Copy options so we can add lang below
    Object.assign(options, this.prefsManager_.speechOptions());
    if (this.enableLanguageDetectionIntegration_ &&
        nodeGroup.detectedLanguage) {
      options.lang = nodeGroup.detectedLanguage;
    }
    if (this.shouldShowNavigationControls_()) {
      options.rate = this.getSpeechRate_();
    }

    const nodeGroupText = nodeGroup.text || '';

    options.onEvent = (event) => {
      if (event.type === 'start' && nodeGroup.nodes.length > 0) {
        this.updatePauseStatusFromTtsEvent_(false /* shouldPause */);

        // Update |this.currentCharIndex_|. Find the first non-space char index
        // in nodeGroup text, or 0 if the text is undefined or the first char is
        // non-space.
        this.currentCharIndex_ = nodeGroupText.search(/\S|$/);

        this.syncCurrentNodeWithCharIndex_(nodeGroup, this.currentCharIndex_);
        if (this.prefsManager_.wordHighlightingEnabled()) {
          // At 'start', find the first word and highlight that. Clear the
          // previous word in the node.
          this.currentNodeWord_ = null;
          // If |this.currentCharIndex_| is not 0, that means we have applied a
          // start offset. Thus, we need to pass startIndexInNodeGroup to
          // opt_startIndex and overwrite the word boundaries in the original
          // node.
          this.updateNodeHighlight_(
              nodeGroupText, this.currentCharIndex_,
              this.currentCharIndex_ !== 0 ? this.currentCharIndex_ :
                                             undefined);
        } else {
          this.testCurrentNode_();
        }
      } else if (event.type === 'interrupted' || event.type === 'cancelled') {
        if (!this.shouldShowNavigationControls_()) {
          this.onStateChanged_(SelectToSpeakState.INACTIVE);
        }
        if (this.state_ === SelectToSpeakState.SELECTING) {
          // Do not go into inactive state if navigation controls are enabled
          // and we're currently making a new selection. This enables users
          // to select new nodes while STS is active without first exiting.
          return;
        }
        if (!this.pauseCompleteCallback_) {
          // Auto dismiss when navigation control is not enabled. In addition,
          // if the interrupted or cancelled events are not triggered by
          // |this.pause_| (e.g., from stopAll_), we should leave STS as
          // INACTIVE. Currently, we check |this.pauseCompleteCallback_| as a
          // proxy to see if the interrupted events are from |this.pause_|.
          this.onStateChanged_(SelectToSpeakState.INACTIVE);
        } else {
          this.updatePauseStatusFromTtsEvent_(true /* shouldPause */);
        }
      } else if (event.type === 'end') {
        this.onNodeGroupSpeakingCompleted_();
      } else if (event.type === 'word') {
        // The Closure compiler doesn't realize that we did a !nodeGroup earlier
        // so we check again here.
        if (!nodeGroup) {
          return;
        }
        this.onTtsWordEvent_(event, nodeGroup);
      }
    };
    chrome.tts.speak(nodeGroupText, options);
  }

  /**
   * When a node group is completed, we start speaking the next node group
   * indicated by the end index. If we have reached the last node group, this
   * function will update STS status depending whether the navigation feature is
   * enabled.
   * @private
   */
  onNodeGroupSpeakingCompleted_() {
    const currentNodeGroup = this.getCurrentNodeGroup_();

    // Update the current char index to the end of the text content in this
    // nodeGroup.
    const nodeGroupText = (currentNodeGroup && currentNodeGroup.text) || '';
    this.currentCharIndex_ = nodeGroupText.trimEnd().length;

    const isLastNodeGroup =
        (this.currentNodeGroupIndex_ === this.currentNodeGroups_.length - 1);
    if (isLastNodeGroup) {
      if (!this.shouldShowNavigationControls_()) {
        this.onStateChanged_(SelectToSpeakState.INACTIVE);
      }
      // If navigation features are enabled, we should turn the pause status to
      // true so that the user can hit resume to continue.
      this.updatePauseStatusFromTtsEvent_(true /* shouldPause */);
      return;
    }

    // Start reading the next node group.
    this.currentNodeGroupIndex_++;
    this.startCurrentNodeGroup_();
  }

  /**
   * Update |this.currentNodeGroupItem_|, the current speaking or the node to be
   * spoken in the node group.
   * @param {ParagraphUtils.NodeGroup} nodeGroup the current nodeGroup.
   * @param {number} charIndex the start char index of the word to be spoken.
   *    The index is relative to the entire NodeGroup.
   * @param {number=} opt_startFromNodeGroupIndex the NodeGroupIndex to start
   *    with. If undefined, search from 0.
   * @return {boolean} if the found NodeGroupIndex is different from the
   *    |opt_startFromNodeGroupIndex|.
   */
  syncCurrentNodeWithCharIndex_(
      nodeGroup, charIndex, opt_startFromNodeGroupIndex) {
    if (opt_startFromNodeGroupIndex === undefined) {
      opt_startFromNodeGroupIndex = 0;
    }

    // There is no speaking word, set the NodeGroupItemIndex to 0.
    if (charIndex <= 0) {
      this.currentNodeGroupItemIndex_ = 0;
      this.currentNodeGroupItem_ =
          nodeGroup.nodes[this.currentNodeGroupItemIndex_];
      return this.currentNodeGroupItemIndex_ === opt_startFromNodeGroupIndex;
    }

    // Sets the |this.currentNodeGroupItemIndex_| to
    // |opt_startFromNodeGroupIndex|
    this.currentNodeGroupItemIndex_ = opt_startFromNodeGroupIndex;
    this.currentNodeGroupItem_ =
        nodeGroup.nodes[this.currentNodeGroupItemIndex_];

    if (this.currentNodeGroupItemIndex_ + 1 < nodeGroup.nodes.length) {
      let next = nodeGroup.nodes[this.currentNodeGroupItemIndex_ + 1];
      let nodeUpdated = false;
      // TODO(katie): For something like a date, the start and end
      // node group nodes can actually be different. Example:
      // "<span>Tuesday,</span> December 18, 2018".

      // Check if we've reached this next node yet. Since charIndex is the
      // start char index of the target word, we just need to make sure the
      // next.startchar is bigger than it.
      while (next && charIndex >= next.startChar &&
             this.currentNodeGroupItemIndex_ + 1 < nodeGroup.nodes.length) {
        next = this.incrementCurrentNodeAndGetNext_(nodeGroup);
        nodeUpdated = true;
      }
      return nodeUpdated;
    }

    return false;
  }

  /**
   * Apply start or end offset to the text of the |nodeGroup|.
   * @param {ParagraphUtils.NodeGroup} nodeGroup the input nodeGroup.
   * @param {number} offset the size of offset.
   * @param {boolean} isStartOffset whether to apply a startOffset or an
   *     endOffset.
   */
  applyOffset(nodeGroup, offset, isStartOffset) {
    if (isStartOffset) {
      // Applying start offset. Remove all text before the start index so that
      // it is not spoken. Backfill with spaces so that index counting
      // functions don't get confused.
      nodeGroup.text = ' '.repeat(offset) + nodeGroup.text.substr(offset);
    } else {
      // Remove all text after the end index so it is not spoken.
      nodeGroup.text = nodeGroup.text.substr(0, offset);
    }
  }

  /**
   * Prepares for speech. Call once before chrome.tts.speak is called.
   * @param {boolean} clearFocusRing Whether to clear the focus ring.
   * @private
   */
  prepareForSpeech_(clearFocusRing) {
    this.cancelIfSpeaking_(clearFocusRing /* clear the focus ring */);
    if (this.intervalRef_ !== undefined) {
      clearInterval(this.intervalRef_);
    }
    this.intervalRef_ = setInterval(
        this.testCurrentNode_.bind(this),
        SelectToSpeakConstants.NODE_STATE_TEST_INTERVAL_MS);
  }

  /**
   * Uses the 'word' speech event to determine which node is currently beings
   * spoken, and prepares for highlight if enabled.
   * @param {!TtsEvent} event The event to use for updates.
   * @param {ParagraphUtils.NodeGroup} nodeGroup The node group for this
   *     utterance.
   * @private
   */
  onTtsWordEvent_(event, nodeGroup) {
    // Not all speech engines include length in the ttsEvent object. .
    const hasLength = event.length !== undefined && event.length >= 0;
    // Only update the |this.currentCharIndex_| if event has a higher charIndex.
    // TTS sometimes will report an incorrect number at the end of an utterance.
    this.currentCharIndex_ = Math.max(event.charIndex, this.currentCharIndex_);
    console.debug(nodeGroup.text + ' (index ' + event.charIndex + ')');
    let debug = '-'.repeat(event.charIndex);
    if (hasLength) {
      debug += '^'.repeat(event.length);
    } else {
      debug += '^';
    }
    console.debug(debug);

    // First determine which node contains the word currently being spoken,
    // and update this.currentNodeGroupItem_, this.currentNodeWord_, and
    // this.currentNodeGroupItemIndex_ to match.
    const nodeUpdated = this.syncCurrentNodeWithCharIndex_(
        nodeGroup, event.charIndex, this.currentNodeGroupItemIndex_);
    if (nodeUpdated) {
      if (!this.prefsManager_.wordHighlightingEnabled()) {
        // If we are doing a per-word highlight, we will test the
        // node after figuring out what the currently highlighted
        // word is. Otherwise, test it now.
        this.testCurrentNode_();
      }
    }

    // Finally update the word highlight if it is enabled.
    if (this.prefsManager_.wordHighlightingEnabled()) {
      if (hasLength) {
        this.currentNodeWord_ = {
          'start': event.charIndex - this.currentNodeGroupItem_.startChar,
          'end': event.charIndex + event.length -
              this.currentNodeGroupItem_.startChar
        };
        this.testCurrentNode_();
      } else {
        this.updateNodeHighlight_(nodeGroup.text, event.charIndex);
      }
    } else {
      this.currentNodeWord_ = null;
      // There are many cases where we won't update the node highlight or test
      // the node. Thus, we need to update the panel independently.
      this.updateNavigationPanel_();
    }
  }

  /**
   * Updates the current node and relevant points to be the next node in the
   * group, then returns the next node in the group after that.
   * @param {!ParagraphUtils.NodeGroup} nodeGroup
   * @return {ParagraphUtils.NodeGroupItem}
   * @private
   */
  incrementCurrentNodeAndGetNext_(nodeGroup) {
    // Move to the next node.
    this.currentNodeGroupItemIndex_ += 1;
    this.currentNodeGroupItem_ =
        nodeGroup.nodes[this.currentNodeGroupItemIndex_];
    // Setting this.currentNodeWord_ to null signals it should be recalculated
    // later.
    this.currentNodeWord_ = null;
    if (this.currentNodeGroupItemIndex_ + 1 >= nodeGroup.nodes.length) {
      return null;
    }
    return nodeGroup.nodes[this.currentNodeGroupItemIndex_ + 1];
  }

  /**
   * Updates the state.
   * @param {!chrome.accessibilityPrivate.SelectToSpeakState} state
   * @private
   */
  onStateChanged_(state) {
    if (this.state_ !== state) {
      if (state === SelectToSpeakState.INACTIVE) {
        this.clearFocusRingAndNode_();
      }
      // Send state change event to Chrome.
      chrome.accessibilityPrivate.setSelectToSpeakState(state);
      this.state_ = state;
    }
  }

  /**
   * Cancels the current speech queue.
   * @param {boolean} clearFocusRing Whether to clear the focus ring
   *    as well.
   * @private
   */
  cancelIfSpeaking_(clearFocusRing) {
    if (clearFocusRing) {
      this.stopAll_();
    } else {
      // Just stop speech
      chrome.tts.stop();
    }
  }

  /**
   * Hides the speech and focus ring states if necessary based on a node's
   * current state.
   *
   * @param {ParagraphUtils.NodeGroupItem} nodeGroupItem The node to use for
   *     updates.
   * @param {boolean} inForeground Whether the node is in the foreground
   *     window.
   * @private
   */
  updateFromNodeState_(nodeGroupItem, inForeground) {
    switch (NodeUtils.getNodeState(nodeGroupItem.node)) {
      case NodeUtils.NodeState.NODE_STATE_INVALID:
        // If the node is invalid, continue speech unless readAfterClose_
        // is set to true. See https://crbug.com/818835 for more.
        if (this.readAfterClose_) {
          this.clearFocusRing_();
          this.visible_ = false;
        } else {
          this.stopAll_();
        }
        break;
      case NodeUtils.NodeState.NODE_STATE_INVISIBLE:
        // If it is invisible but still valid, just clear the focus ring.
        // Don't clear the current node because we may still use it
        // if it becomes visibile later.
        this.clearFocusRing_();
        this.visible_ = false;
        break;
      case NodeUtils.NodeState.NODE_STATE_NORMAL:
      default:
        if (inForeground && !this.visible_) {
          this.visible_ = true;
          // Just came to the foreground.
          this.updateHighlightAndFocus_(nodeGroupItem);
        } else if (!inForeground) {
          this.clearFocusRing_();
          this.visible_ = false;
        }
    }
  }

  /**
   * Updates the speech and focus ring states based on a node's current state.
   *
   * @param {ParagraphUtils.NodeGroupItem} nodeGroupItem The node to use for
   *    updates.
   * @private
   */
  updateHighlightAndFocus_(nodeGroupItem) {
    if (!this.visible_) {
      return;
    }
    let node;
    if (nodeGroupItem.hasInlineText && this.currentNodeWord_) {
      node = ParagraphUtils.findInlineTextNodeByCharacterIndex(
          nodeGroupItem.node, this.currentNodeWord_.start);
    } else if (
        nodeGroupItem.hasInlineText && this.shouldShowNavigationControls_()) {
      // If navigation controls are enabled, but word highlighting is disabled
      // (currentNodeWord_ === null), still find the inline text node so the
      // focus ring will highlight the whole block.
      node = ParagraphUtils.findInlineTextNodeByCharacterIndex(
          nodeGroupItem.node, 0);
    } else {
      // No inline text or word highlighting and navigation controls are
      // disabled.
      node = nodeGroupItem.node;
    }
    if (this.scrollToSpokenNode_ && node.state.offscreen) {
      node.makeVisible();
    }
    if (this.prefsManager_.wordHighlightingEnabled() &&
        this.currentNodeWord_ != null) {
      var charIndexInParent = 0;
      // getStartCharIndexInParent is only defined for nodes with role
      // INLINE_TEXT_BOX.
      if (node.role === RoleType.INLINE_TEXT_BOX) {
        charIndexInParent = ParagraphUtils.getStartCharIndexInParent(node);
      }
      node.boundsForRange(
          this.currentNodeWord_.start - charIndexInParent,
          this.currentNodeWord_.end - charIndexInParent, (bounds) => {
            if (bounds) {
              chrome.accessibilityPrivate.setHighlights(
                  [bounds], this.prefsManager_.highlightColor());
            } else {
              chrome.accessibilityPrivate.setHighlights(
                  [], this.prefsManager_.highlightColor());
            }
          });
    }
    // Show the parent element of the currently verbalized node with the
    // focus ring. This is a nicer user-facing behavior than jumping from
    // node to node, as nodes may not correspond well to paragraphs or
    // blocks.
    // TODO: Better test: has no siblings in the group, highlight just
    // the one node. if it has siblings, highlight the parent.
    let focusRingRect;
    const currentNodeGroup = this.getCurrentNodeGroup_();
    if (!currentNodeGroup) {
      return;
    }
    const currentBlockParent = currentNodeGroup.blockParent;
    if (currentBlockParent !== null && node.role === RoleType.INLINE_TEXT_BOX) {
      focusRingRect = currentBlockParent.location;
    } else {
      focusRingRect = node.location;
    }
    this.setFocusRings_([focusRingRect], true /* draw background */);
    this.updateNavigationPanel_();
  }

  /**
   * Tests the active node to make sure the bounds are drawn correctly.
   * @private
   */
  testCurrentNode_() {
    if (this.currentNodeGroupItem_ == null) {
      return;
    }
    if (this.currentNodeGroupItem_.node.location === undefined) {
      // Don't do the hit test because there is no location to test against.
      // Just directly update Select To Speak from node state.
      this.updateFromNodeState_(this.currentNodeGroupItem_, false);
    } else {
      this.updateHighlightAndFocus_(this.currentNodeGroupItem_);
      // Do a hit test to make sure the node is not in a background window
      // or minimimized. On the result checkCurrentNodeMatchesHitTest_ will be
      // called, and we will use that result plus the currentNode's state to
      // determine how to set the focus and whether to stop speech.
      this.desktop_.hitTest(
          this.currentNodeGroupItem_.node.location.left,
          this.currentNodeGroupItem_.node.location.top, EventType.HOVER);
    }
  }

  /**
   * Checks that the current node is in the same window as the HitTest node.
   * Uses this information to update Select-To-Speak from node state.
   * @private
   */
  onHitTestCheckCurrentNodeMatches_(evt) {
    if (this.currentNodeGroupItem_ == null) {
      return;
    }
    chrome.automation.getFocus(function(focusedNode) {
      var window = NodeUtils.getNearestContainingWindow(evt.target);
      var currentWindow =
          NodeUtils.getNearestContainingWindow(this.currentNodeGroupItem_.node);
      var inForeground =
          currentWindow != null && window != null && currentWindow === window;
      if (!inForeground &&
          (this.isPanel_(window) ||
           this.isPanel_(NodeUtils.getNearestContainingWindow(focusedNode)))) {
        // If the focus is on the Select-to-speak panel or the hit test landed
        // on the panel, treat the current node as if it is in the foreground.
        inForeground = true;
      }
      if (!inForeground && focusedNode && currentWindow) {
        // See if the focused node window matches the currentWindow.
        // This may happen in some cases, for example, ARC++, when the window
        // which received the hit test request is not part of the tree that
        // contains the actual content. In such cases, use focus to get the
        // appropriate root.
        var focusedWindow =
            NodeUtils.getNearestContainingWindow(focusedNode.root);
        inForeground = focusedWindow != null && currentWindow === focusedWindow;
      }
      this.updateFromNodeState_(this.currentNodeGroupItem_, inForeground);
    }.bind(this));
  }

  /**
   * @param {?AutomationNode|undefined} node
   * @return {boolean} Whether given node is the Select-to-speak floating panel.
   * @private
   */
  isPanel_(node) {
    if (!node) {
      return false;
    }
    if (node === this.panel_) {
      return true;
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
   * Updates the currently highlighted node word based on the current text
   * and the character index of an event.
   * @param {string} text The current text
   * @param {number} charIndex The index of a current event in the text.
   * @param {number=} opt_startIndex The index at which to start the
   *     highlight. This takes precedence over the charIndex.
   * @private
   */
  updateNodeHighlight_(text, charIndex, opt_startIndex) {
    if (charIndex >= text.length) {
      // No need to do work if we are at the end of the paragraph.
      return;
    }
    // Get the next word based on the event's charIndex.
    const nextWordStart =
        WordUtils.getNextWordStart(text, charIndex, this.currentNodeGroupItem_);
    // The |WordUtils.getNextWordEnd| will find the correct end based on the
    // trimmed text, so there is no need to provide additional input like
    // opt_startIndex.
    const nextWordEnd = WordUtils.getNextWordEnd(
        text, opt_startIndex === undefined ? nextWordStart : opt_startIndex,
        this.currentNodeGroupItem_);
    // Map the next word into the node's index from the text.
    const nodeStart = opt_startIndex === undefined ?
        nextWordStart - this.currentNodeGroupItem_.startChar :
        opt_startIndex - this.currentNodeGroupItem_.startChar;
    const nodeEnd = Math.min(
        nextWordEnd - this.currentNodeGroupItem_.startChar,
        NodeUtils.nameLength(this.currentNodeGroupItem_.node));
    if ((this.currentNodeWord_ == null ||
         nodeStart >= this.currentNodeWord_.end) &&
        nodeStart <= nodeEnd) {
      // Only update the bounds if they have increased from the
      // previous node. Because tts may send multiple callbacks
      // for the end of one word and the beginning of the next,
      // checking that the current word has changed allows us to
      // reduce extra work.
      this.currentNodeWord_ = {'start': nodeStart, 'end': nodeEnd};
      this.testCurrentNode_();
    }
  }

  /**
   * @return {number} Current speech rate.
   * @private
   */
  getSpeechRate_() {
    return this.overrideSpeechRate_ || this.systemSpeechRate_;
  }

  /**
   * @param {!Array<!AutomationNode>} nodes
   * @return {boolean} Whether all given nodes support the navigation panel.
   * @private
   */
  isNavigationPanelSupported_(nodes) {
    if (nodes.length === 0) {
      return true;
    }
    // Do not show panel on system UI. System UI can be problematic due to
    // auto-dismissing behavior (see http://crbug.com/1157148), but also
    // navigation controls do not work well control-rich interfaces that are
    // light on text (and therefore sentence and paragraph structures).
    return !nodes.some((n) => n.root && n.root.role === RoleType.DESKTOP);
  }

  /**
   * Fires a mock key down event for testing.
   * @param {!Event} event The fake key down event to fire. The object
   * must contain at minimum a keyCode.
   * @protected
   */
  fireMockKeyDownEvent(event) {
    this.inputHandler_.onKeyDown_(event);
  }

  /**
   * Fires a mock key up event for testing.
   * @param {!Event} event The fake key up event to fire. The object
   * must contain at minimum a keyCode.
   * @protected
   */
  fireMockKeyUpEvent(event) {
    this.inputHandler_.onKeyUp_(event);
  }

  /**
   * Fires a mock mouse down event for testing.
   * @param {!Event} event The fake mouse down event to fire. The object
   * must contain at minimum a screenX and a screenY.
   * @protected
   */
  fireMockMouseDownEvent(event) {
    this.inputHandler_.onMouseDown_(event);
  }

  /**
   * Fires a mock mouse up event for testing.
   * @param {!Event} event The fake mouse up event to fire. The object
   * must contain at minimum a screenX and a screenY.
   * @protected
   */
  fireMockMouseUpEvent(event) {
    this.inputHandler_.onMouseUp_(event);
  }
}
