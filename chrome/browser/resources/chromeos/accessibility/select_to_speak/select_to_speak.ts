// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {constants} from '/common/constants.js';
import {FlagName, Flags} from '/common/flags.js';
import {NodeNavigationUtils} from '/common/node_navigation_utils.js';
import {NodeUtils} from '/common/node_utils.js';
import {ParagraphUtils} from '/common/paragraph_utils.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import {WordUtils} from '/common/word_utils.js';

import {InputHandler} from './input_handler.js';
import {MetricsUtils} from './metrics_utils.js';
import {PrefsManager} from './prefs_manager.js';
import {SelectToSpeakConstants} from './select_to_speak_constants.js';
import {TtsManager} from './tts_manager.js';
import {SelectToSpeakUiListener, UiManager} from './ui_manager.js';

type AutomationNode = chrome.automation.AutomationNode;
import AutomationEvent = chrome.automation.AutomationEvent;
import EventType = chrome.automation.EventType;
import RoleType = chrome.automation.RoleType;
import SelectToSpeakState = chrome.accessibilityPrivate.SelectToSpeakState;

// Matches one of the known GSuite apps which need the clipboard to find and
// read selected text. Includes sandbox and non-sandbox versions.
const GSUITE_APP_REGEXP =
    /^https:\/\/docs\.(?:sandbox\.)?google\.com\/(?:(?:presentation)|(?:document)|(?:spreadsheets)|(?:drawings)|(?:scenes)){1}\//;

/**
 * Determines if a node is in one of the known Google GSuite apps that needs
 * special case treatment for speaking selected text. Not all Google GSuite
 * pages are included, because some are not known to have a problem with
 * selection: Forms is not included since it's relatively similar to any HTML
 * page, for example.
 * @param node The node to check
 * @return The root node of the GSuite app, or null if none is found.
 */
export function getGSuiteAppRoot(node: AutomationNode|
                                 undefined): AutomationNode|null {
  while (node !== undefined && node.root !== undefined) {
    if (node.root.url !== undefined && GSUITE_APP_REGEXP.exec(node.root.url)) {
      return node.root;
    }
    node = node.root.parent;
  }
  return null;
}

/**
 * Select-to-speak component extension controller.
 */
export class SelectToSpeak implements SelectToSpeakUiListener {
  private currentCharIndex_: number;
  private currentNodeGroupIndex_: number;
  // TODO(b/314203187): In many places we've added a currentNodeGroupItem_!,
  // determine if this is correct or if a check should be added.
  private currentNodeGroupItem_: ParagraphUtils.NodeGroupItem|null;
  private currentNodeGroupItemIndex_: number;
  private currentNodeGroups_: ParagraphUtils.NodeGroup[];
  private currentNodeWord_: {start: number, end: number}|null;
  private desktop_: AutomationNode|undefined;
  private inputHandler_: InputHandler|null;
  private intervalId_: number|undefined;
  private nullSelectionTone_: HTMLAudioElement;
  private onStateChangeRequestedCallbackForTest_: (() => void)|null;
  private prefsManager_: PrefsManager;
  private scrollToSpokenNode_: boolean;
  private speechRateMultiplier_: number;
  private state_: SelectToSpeakState;
  private supportsNavigationPanel_: boolean;
  private ttsManager_: TtsManager;
  private uiManager_: UiManager;
  private onLoadDesktopCallbackForTest_: (() => void)|null;

  /** Please keep fields in alphabetical order. */
  constructor() {
    /**
     * The start char index of the word to be spoken. The index is relative
     * to the text content of the current node group.
     */
    this.currentCharIndex_ = -1;

    /**
     * The index for the node group currently being spoken in
     * |this.currentNodeGroups_|.
     */
    this.currentNodeGroupIndex_ = -1;

    /**
     * The node group item currently being spoken. A node group item is a
     * representation of the original input nodes, but may not be the same. For
     * example, an input inline text node will be represented by its static text
     * node in the node group item.
     */
    this.currentNodeGroupItem_ = null;

    /**
     * The index for the current node group item within the current node group,
     * The current node group can be accessed from |this.currentNodeGroups_|
     * using |this.currentNodeGroupIndex_|. In most cases,
     * |this.currentNodeGroupItemIndex_| can be used to get
     * |this.currentNodeGroupItem_| from the current node group. However, in
     * Gsuite, we will have node group items outside of a node group.
     */
    this.currentNodeGroupItemIndex_ = -1;

    /**
     * The node groups to be spoken. We process content into node groups and
     * pass one node group at a time to the TTS engine. Note that we do not use
     * node groups for user-selected text in Gsuite. See more details in
     * readNodesBetweenPositions_.
     */
    this.currentNodeGroups_ = [];

    /**
     * The indexes within the current node group item representing the word
     * currently being spoken. Only updated if word highlighting is enabled.
     */
    this.currentNodeWord_ = null;

    this.desktop_;

    this.inputHandler_ = null;

    /**
     * The interval ID from a call to setInterval, which is set whenever
     * speech is in progress.
     */
    this.intervalId_;

    this.nullSelectionTone_ = new Audio('earcons/null_selection.ogg');

    /**
     * Function to be called when a state change request is received from the
     * accessibilityPrivate API.
     */
    this.onStateChangeRequestedCallbackForTest_ = null;

    this.prefsManager_ = new PrefsManager();

    this.scrollToSpokenNode_ = false;

    /** Speech rate multiplier. */
    this.speechRateMultiplier_ = 1.0;

    /**
     * The current state of the SelectToSpeak extension, from
     * SelectToSpeakState.
     */
    this.state_ = SelectToSpeakState.INACTIVE;

    /**
     * Whether the current nodes support use of the navigation panel.
     */
    this.supportsNavigationPanel_ = true;

    this.ttsManager_ = new TtsManager();

    this.uiManager_ = new UiManager(this.prefsManager_, /*listener=*/ this);

    this.onLoadDesktopCallbackForTest_ = null;

    this.init_();
  }

  private async init_(): Promise<void> {
    chrome.automation.getDesktop(desktop => {
      this.desktop_ = desktop;

      // After the user selects a region of the screen, we do a hit test at
      // the center of that box using the automation API. The result of the
      // hit test is a MOUSE_RELEASED accessibility event.
      desktop.addEventListener(
          EventType.MOUSE_RELEASED, evt => this.onAutomationHitTest_(evt),
          true);
      // Chrome PDF Viewer with PDF OCR sends a layout complete event when
      // finishing extracting text from inaccessible PDF pages.
      desktop.addEventListener(
          EventType.LAYOUT_COMPLETE, evt => this.onLayoutComplete_(evt), true);

      if (this.onLoadDesktopCallbackForTest_) {
        this.onLoadDesktopCallbackForTest_();
        this.onLoadDesktopCallbackForTest_ = null;
      }
    });

    this.prefsManager_.initPreferences();

    this.runContentScripts_();
    this.setUpEventListeners_();

    await Flags.init();
    const createArgs: chrome.contextMenus.CreateProperties = {
      title: chrome.i18n.getMessage(
          'select_to_speak_listen_context_menu_option_text'),
      contexts: [chrome.contextMenus.ContextType.SELECTION],
      id: 'select_to_speak',
    };
    if (Flags.isEnabled(FlagName.MANIFEST_V3)) {
      chrome.contextMenus.onClicked.addListener(() => {
        this.getFocusedNodeAndSpeakSelectedText_();
      });
    } else {
      createArgs['onclick'] = () => {
        this.getFocusedNodeAndSpeakSelectedText_();
      };
    }
    // Install the context menu in the Ash browser.
    await chrome.contextMenus.create(createArgs);

    // Listen for context menu clicks from other contexts (like Lacros).
    chrome.accessibilityPrivate.onSelectToSpeakContextMenuClicked.addListener(
        () => {
          this.getFocusedNodeAndSpeakSelectedText_();
        });
  }

  /**
   * Gets the node group currently being spoken.
   */
  private getCurrentNodeGroup_(): ParagraphUtils.NodeGroup|undefined {
    if (this.currentNodeGroups_.length === 0) {
      return undefined;
    }
    return this.currentNodeGroups_[this.currentNodeGroupIndex_];
  }

  /**
   * Determines if navigation controls should be shown (and other related
   * functionality, such as auto-dismiss and click-to-navigate to sentence,
   * should be activated) based on feature flag and user setting.
   */
  private shouldShowNavigationControls_(): boolean {
    return this.prefsManager_.navigationControlsEnabled() &&
        this.supportsNavigationPanel_;
  }

  /**
   * Read the status message under the status node in a PDF accessibility tree
   * if PDF content is still being loaded. In the loading phase, the PDF a11y
   * tree will have one child node with the banner role, which contains the
   * loading status message as follows:
   * pdfRoot
   * - banner
   * -- status
   * --- staticText: "Loading PDF"
   */
  private readPdfStatusNodeIfStillLoading_(pdfRoot: AutomationNode): boolean {
    if (pdfRoot.role === RoleType.PDF_ROOT && pdfRoot.children.length === 1 &&
        pdfRoot.firstChild!.role === RoleType.BANNER &&
        pdfRoot.firstChild!.children.length === 1 &&
        pdfRoot.firstChild!.firstChild!.role === RoleType.STATUS &&
        pdfRoot.firstChild!.firstChild!.children.length === 1 &&
        pdfRoot.firstChild!.firstChild!.firstChild!.role ===
            RoleType.STATIC_TEXT) {
      this.startSpeechQueue_([pdfRoot.firstChild!.firstChild!.firstChild!], {
        clearFocusRing: true,
      });
      return true;
    }
    return false;
  }

  private onLayoutComplete_(evt: AutomationEvent): void {
    const root: AutomationNode = evt.target;
    if (!root.url || !root.url.endsWith('.pdf')) {
      return;
    }

    // Check if it's Chrome PDF Viewer with PDF OCR in the full-page view.
    const pdfRoot: AutomationNode|null = root.find({role: RoleType.PDF_ROOT});
    if (!pdfRoot) {
      return;
    }

    this.recordOcredPagesInPdf_(pdfRoot);
  }

  /**
   * Record the number of OCRed pages in the PDF accessibility tree.
   */
  private recordOcredPagesInPdf_(pdfRoot: AutomationNode): void {
    // When PDF OCR successfully extracts text from inaccessible PDF pages, PDF
    // pages with OCRed content will have the "ocred_page" class name.
    const orcedPages = pdfRoot.findAll({attributes: {className: 'ocred_page'}});
    MetricsUtils.recordNumPdfPagesOcred(orcedPages.length);
  }

  /**
   * Called in response to our hit test after the mouse is released,
   * when the user is in a mode where Select-to-speak is capturing
   * mouse events (for example holding down Search).
   * @param evt The automation event from the hit test.
   */
  private onAutomationHitTest_(evt: AutomationEvent): void {
    // Walk up to the nearest window, web area, toolbar, or dialog that the
    // hit node is contained inside. Only speak objects within that
    // container. In the future we might include other container-like
    // roles here.
    var root = evt.target;

    // In Chrome PDF Viewer, PDF content for a large PDF might be still being
    // loaded into a PDF accessibility tree when the user selects text on a PDF
    // page. In this case, the PDF root has only one child node, which is the
    // status node that contains a loading status message. Read this status
    // message if the user tries selecting text during this loading phase.
    if (root.role === RoleType.EMBEDDED_OBJECT && root.children.length === 1 &&
        root.firstChild!.role === RoleType.PDF_ROOT &&
        root.firstChild!.children.length === 1 &&
        this.readPdfStatusNodeIfStillLoading_(root.firstChild!)) {
      return;
    }

    // TODO: Use AutomationPredicate.root instead?
    while (root.parent && root.role !== RoleType.WINDOW &&
           root.role !== RoleType.ROOT_WEB_AREA &&
           root.role !== RoleType.DESKTOP && root.role !== RoleType.DIALOG &&
           root.role !== RoleType.ALERT_DIALOG &&
           root.role !== RoleType.TOOLBAR) {
      root = root.parent;
    }

    var rect = this.inputHandler_!.getMouseRect();
    var nodes: AutomationNode[] = [];
    chrome.automation.getFocus(focusedNode => {
      // In some cases, e.g. ARC++, the window received in the hit test request,
      // which is computed based on which window is the event handler for the
      // hit point, isn't the part of the tree that contains the actual
      // content. In such cases, use focus to get the root.
      // TODO(katie): Determine if this work-around needs to be ARC++ only. If
      // so, look for classname exoshell on the root or root parent to confirm
      // that a node is in ARC++.
      if (!NodeUtils.findAllMatching(root, rect, nodes) && focusedNode &&
          focusedNode.root!.role !== RoleType.DESKTOP) {
        // TODO(b/314203187): Determine if not null assertion is appropriate
        // here.
        NodeUtils.findAllMatching(focusedNode.root!, rect, nodes);
      }
      if (nodes.length === 1 && UiManager.isTrayButton(nodes[0])) {
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
      this.startSpeechQueue_(nodes, {
        clearFocusRing: true,
      });
      MetricsUtils.recordStartEvent(
          MetricsUtils.StartSpeechMethod.MOUSE, this.prefsManager_);
    });
  }

  private getFocusedNodeAndSpeakSelectedText_(): void {
    chrome.automation.getFocus(
        focusedNode => this.requestSpeakSelectedText_(
            MetricsUtils.StartSpeechMethod.CONTEXT_MENU, focusedNode));
  }

  /**
   * Queues up selected text for reading by finding the Position objects
   * representing the selection.
   * @param method the method that
   *     caused the text to speak.
   */
  private requestSpeakSelectedText_(
      method: MetricsUtils.StartSpeechMethod,
      focusedNode: AutomationNode): void {
    // If nothing is selected, return early. Check if the focused node has
    // textSelStart and textSelEnd. For native UI like the omnibox, the root
    // might not have a selectionStartObject and selectionEndObject. Therefore
    // we must check textSelStart and textSelEnd on the focused node.
    if (!focusedNode || !focusedNode.root) {
      this.onNullSelection_();
      return;
    }
    const hasSelectionObjects = focusedNode.root.selectionStartObject &&
        focusedNode.root.selectionEndObject;
    const hasTextSelection = focusedNode.textSelStart !== undefined &&
        focusedNode.textSelEnd !== undefined;
    if (!hasSelectionObjects && !hasTextSelection) {
      this.onNullSelection_();
      return;
    }

    let startObject;
    let startOffset = 0;
    let endObject;
    let endOffset = 0;
    // Use selectionStartObject/selectionEndObject if available. Otherwise,
    // use textSelStart/textSelEnd to get the selection offset.
    if (hasSelectionObjects) {
      startObject = focusedNode.root.selectionStartObject;
      startOffset = focusedNode.root.selectionStartOffset || 0;
      endObject = focusedNode.root.selectionEndObject;
      endOffset = focusedNode.root.selectionEndOffset || 0;
    } else if (hasTextSelection) {
      startObject = focusedNode;
      startOffset = focusedNode.textSelStart || 0;
      endObject = focusedNode;
      endOffset = focusedNode.textSelEnd || 0;
    }

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

    const startPosition = NodeUtils.getDeepEquivalentForSelection(
        startObject!, startOffset, true);
    const endPosition =
        NodeUtils.getDeepEquivalentForSelection(endObject!, endOffset, false);

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
    this.readNodesBetweenPositions_(
        firstPosition, lastPosition, method, focusedNode);
  }

  /**
   * Reads nodes between positions.
   * @param firstPosition The first position at which to start reading.
   * @param lastPosition The last position at which to stop reading.
   * @param method the method used to
   *     activate the speech, null if not actived by user.
   * @param focusedNode The node with user focus.
   */
  private readNodesBetweenPositions_(
      firstPosition: NodeUtils.Position, lastPosition: NodeUtils.Position,
      method: MetricsUtils.StartSpeechMethod|null,
      focusedNode: AutomationNode|undefined): void {
    const nodes = [];
    // TODO(b/314204374): AutomationUtil.findNextNode may return null.
    let selectedNode: AutomationNode|null = firstPosition.node;
    // If the method is set, a user requested the speech.
    const userRequested = method !== null;
    const methodNumber: number = method !== null ? method : -1;
    // Certain nodes such as omnibox store text value in the value property,
    // instead of the name property. The getNodeName method in ParagraphUtils
    // does handle this case properly, so use this static method to get text
    // from either `name' or `value' of the node.
    const nodeName = ParagraphUtils.getNodeName(selectedNode);
    if (nodeName && firstPosition.offset < nodeName.length &&
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
          clearFocusRing: userRequested,
          startCharIndex: firstPosition.offset,
        });
      } else {
        this.startSpeechQueue_(nodes, {
          clearFocusRing: userRequested,
          startCharIndex: firstPosition.offset,
          endCharIndex: lastPosition.offset,
        });
      }
      if (focusedNode) {
        this.initializeScrollingToOffscreenNodes_(focusedNode.root);
      }
      if (userRequested) {
        MetricsUtils.recordStartEvent(methodNumber, this.prefsManager_);
      }
    } else {
      // Gsuite apps include webapps beyond Docs, see getGSuiteAppRoot and
      // GSUITE_APP_REGEXP.
      const gsuiteAppRootNode = getGSuiteAppRoot(focusedNode);
      if (!gsuiteAppRootNode) {
        return;
      }
      chrome.tabs.query({active: true}, tabs => {
        // Closure doesn't realize that we did a !gsuiteAppRootNode earlier
        // so we check again here.
        if (!gsuiteAppRootNode || gsuiteAppRootNode.url === undefined) {
          return;
        }
        this.inputHandler_!.onRequestReadClipboardData();
        this.currentNodeGroupItem_ =
            new ParagraphUtils.NodeGroupItem(gsuiteAppRootNode, 0, false);
        if (tabs.length > 0 && tabs[0].url === gsuiteAppRootNode.url) {
          const tab = tabs[0];
          chrome.tabs.executeScript(tab.id, {
            allFrames: true,
            matchAboutBlank: true,
            code: 'document.execCommand("copy");',
          });
        } else {
          // In Lacros because chrome.tabs didn't return a tab or it
          // was a tab with a different URL.
          chrome.accessibilityPrivate.clipboardCopyInActiveLacrosGoogleDoc(
              gsuiteAppRootNode.url);
        }
        if (userRequested) {
          MetricsUtils.recordStartEvent(methodNumber, this.prefsManager_);
        }
      });
    }
  }

  /**
   * Gets ready to cancel future scrolling to offscreen nodes as soon as
   * a user-initiated scroll is done.
   * @param root The root node to listen for events on.
   */
  private initializeScrollingToOffscreenNodes_(root: AutomationNode|
                                               undefined): void {
    if (!root) {
      return;
    }
    this.scrollToSpokenNode_ = true;
    const listener = (event: chrome.automation.AutomationEvent): void => {
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
   */
  private onNullSelection_(): void {
    if (!this.shouldShowNavigationControls_()) {
      this.nullSelectionTone_.play();
      return;
    }

    this.uiManager_.setFocusToPanel();
  }

  /**
   * Whether the STS is on a pause state, where |this.ttsManager_.isSpeaking| is
   * false and |this.state_| is SPEAKING.
   * TODO(leileilei): use two SelectToSpeak states to differentiate speaking and
   * pausing with panel.
   */
  private isPaused_(): boolean {
    return !this.ttsManager_.isSpeaking() &&
        this.state_ === SelectToSpeakState.SPEAKING;
  }

  /**
   * Pause the TTS.
   */
  private pause_(): Promise<any> {
    return this.ttsManager_.pause();
  }

  /**
   * Resume the TTS.
   */
  private resume_(): void {
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

    this.ttsManager_.resume(this.getTtsOptionsForCurrentNodeGroup_());
  }

  /**
   * If resume is successful, a resume event will be sent. We use this event to
   * update node state.
   */
  private onTtsResumeSucceedEvent_(event: chrome.tts.TtsEvent): void {
    // If the node group is invalid, ignore the resume event. This is not
    // expected.
    const currentNodeGroup = this.getCurrentNodeGroup_();
    if (!currentNodeGroup) {
      console.warn('Unexpected invalid node group on TTS resume event.');
      return;
    }
    this.onTtsWordEvent_(event, currentNodeGroup);
  }

  /**
   * When resuming with empty content, an error event will be sent. If there
   * is no remaining user-selected content, STS will read from the current
   * position to the end of the current paragraph. If there is no content left
   * in this paragraph, we navigate to the next paragraph.
   */
  private onTtsResumeErrorEvent_(_event: chrome.tts.TtsEvent): void {
    // If the node group is invalid, ignore the error event. This is not
    // expected.
    const currentNodeGroup = this.getCurrentNodeGroup_();
    if (!currentNodeGroup) {
      console.warn(
          'Unexpected invalid node group on TTS error event when resuming.');
      return;
    }

    // STS should try to read from the current position to the end of the
    // current paragraph. First, we get the current position. If we do not find
    // a position based on the |this.currentCharIndex_|, that means we have
    // reached the end of current node group. We fallback to the end position.
    const currentPosition = NodeUtils.getPositionFromNodeGroup(
        currentNodeGroup, this.currentCharIndex_, true /* fallbackToEnd */);

    // If we have passed the user-selected content, STS should speak the content
    // from the current position to the end of the current node group.
    const {nodes: remainingNodes, offset} =
        NodeNavigationUtils.getNextNodesInParagraphFromPosition(
            currentPosition, constants.Dir.FORWARD);

    // If there is no remaining nodes in this paragraph, we navigate to the next
    // paragraph.
    if (remainingNodes.length === 0) {
      this.navigateToNextParagraph_(constants.Dir.FORWARD);
      return;
    }

    this.startSpeechQueue_(remainingNodes, {
      clearFocusRing: false,
      startCharIndex: offset,
    });
  }

  /**
   * Stop speech. If speech was in-progress, the interruption
   * event will be caught and clearFocusRingAndNode_ will be
   * called, stopping visual feedback as well.
   * If speech was not in progress, i.e. if the user was drawing
   * a focus ring on the screen, this still clears the visual
   * focus ring.
   */
  private stopAll_(): void {
    this.ttsManager_.stop();
    this.uiManager_.clear();
    this.onStateChanged_(SelectToSpeakState.INACTIVE);
  }

  /**
   * Clears the current focus ring and node, but does
   * not stop the speech.
   */
  private clearFocusRingAndNode_(): void {
    this.uiManager_.clear();
    // Clear the node and also stop the interval testing.
    this.resetNodes_();
    this.supportsNavigationPanel_ = true;
    if (this.intervalId_ !== undefined) {
      clearInterval(this.intervalId_);
      this.intervalId_ = undefined;
    }
    this.scrollToSpokenNode_ = false;
  }

  /**
   * Resets the instance variables for nodes and node groups.
   */
  private resetNodes_(): void {
    this.currentNodeGroups_ = [];
    this.currentNodeGroupIndex_ = -1;
    this.currentNodeGroupItem_ = null;
    this.currentNodeGroupItemIndex_ = -1;
    this.currentNodeWord_ = null;
    this.currentCharIndex_ = -1;
  }

  /**
   * Runs content scripts that allow Select-to-Speak access to
   * Google Docs content without a11y mode enabled, in every open
   * tab. Should be run when Select-to-Speak starts up so that any
   * tabs already opened will be checked.
   * This should be kept in sync with the "content_scripts" section in
   * the Select-to-Speak manifest.
   */
  private runContentScripts_(): void {
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
            'https://docs.sandbox.google.com/*',
          ],
        },
        tabs => {
          tabs.forEach(tab => {
            chrome.tabs.executeScript(tab.id, {file: script});
          });
        });
  }

  /**
   * Set up event listeners user input.
   */
  private setUpEventListeners_(): void {
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
          this.desktop_!.hitTest(x, y, EventType.MOUSE_PRESSED);
        } else {
          this.onStateChanged_(SelectToSpeakState.INACTIVE);
          // Do a hit test at the center of the area the user dragged over.
          // This will give us some context when searching the accessibility
          // tree. The hit test will result in a EventType.MOUSE_RELEASED
          // event being fired on the result of that hit test, which will
          // trigger onAutomationHitTest_.
          this.desktop_!.hitTest(x, y, EventType.MOUSE_RELEASED);
        }
      },
      // onSelectionChanged: Mouse selection rect changed.
      onSelectionChanged: rect => {
        this.uiManager_.setSelectionRect(rect);
      },
      // onKeystrokeSelection: Keys pressed for reading highlighted text.
      onKeystrokeSelection: () => {
        chrome.automation.getFocus(
            focusedNode => this.requestSpeakSelectedText_(
                MetricsUtils.StartSpeechMethod.KEYSTROKE, focusedNode));
      },
      // onRequestCancel: User requested canceling input/speech.
      onRequestCancel: () => {
        // User manually requested cancel, so log cancel metric.
        MetricsUtils.recordCancelIfSpeaking();
        this.cancelIfSpeaking_(true /* clear the focus ring */);
      },
      // onTextReceived: Text received from a 'paste' event to read aloud.
      onTextReceived: text => this.startSpeech_(text),
    });
    this.inputHandler_.setUpEventListeners();

    // Initialize the state to SelectToSpeakState.INACTIVE.
    chrome.accessibilityPrivate.setSelectToSpeakState(this.state_);
  }

  /**
   * Called when Chrome OS is requesting Select-to-Speak to switch states.
   */
  onStateChangeRequested(): void {
    // Switch Select-to-Speak states on request.
    // We will need to track the current state and toggle from one state to
    // the next when this function is called, and then call
    // accessibilityPrivate.setSelectToSpeakState with the new state.
    switch (this.state_) {
      case SelectToSpeakState.INACTIVE:
        // Start selection.
        this.inputHandler_!.setTrackingMouse(true);
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
        this.inputHandler_!.setTrackingMouse(false);
        this.onStateChanged_(SelectToSpeakState.INACTIVE);
        MetricsUtils.recordSelectToSpeakStateChangeEvent(
            MetricsUtils.StateChangeEvent.CANCEL_SELECTION);
    }
    this.onStateChangeRequestedCallbackForTest_ &&
        this.onStateChangeRequestedCallbackForTest_();
  }

  /** Handles user request to navigate to next paragraph. */
  onNextParagraphRequested(): void {
    this.navigateToNextParagraph_(constants.Dir.FORWARD);
  }

  /** Handles user request to navigate to previous paragraph. */
  onPreviousParagraphRequested(): void {
    this.navigateToNextParagraph_(constants.Dir.BACKWARD);
  }

  /** Handles user request to navigate to next sentence. */
  onNextSentenceRequested(): void {
    this.navigateToNextSentence_(constants.Dir.FORWARD);
  }

  /** Handles user request to navigate to previous sentence. */
  onPreviousSentenceRequested(): void {
    this.navigateToNextSentence_(constants.Dir.BACKWARD);
  }

  /** Handles user request to navigate to exit STS. */
  onExitRequested(): void {
    // User manually requested, so log cancel metric.
    MetricsUtils.recordCancelIfSpeaking();
    this.stopAll_();
  }

  /** Handles user request to pause TTS. */
  onPauseRequested(): void {
    MetricsUtils.recordPauseEvent();
    this.pause_();
  }

  /** Handles user request to resume TTS. */
  onResumeRequested(): void {
    if (this.isPaused_()) {
      MetricsUtils.recordResumeEvent();
      this.resume_();
    }
  }

  /**
   * Handles user request to adjust reading speed.
   */
  onChangeSpeedRequested(rateMultiplier: number): void {
    this.speechRateMultiplier_ = rateMultiplier;
    // If currently playing, stop TTS, then resume from current spot.
    if (!this.isPaused_()) {
      this.pause_().then(() => {
        this.resume_();
      });
    }
  }

  /**
   * Navigates to the next sentence.
   * @param direction Direction to search for the next sentence.
   *     If set to forward, we look for the sentence start after the current
   *     position. Otherwise, we look for the sentence start before the current
   *     position.
   */
  private async navigateToNextSentence_(direction: constants.Dir):
      Promise<void> {
    if (!this.isPaused_()) {
      await this.pause_();
    }
    const {nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
        this.getCurrentNodeGroup_(), this.currentCharIndex_, direction,
        nodes => this.skipPanel_(nodes));
    if (nodes.length === 0) {
      return;
    }
    // Ensure the first node in the paragraph is visible.
    nodes[0].makeVisible();

    this.startSpeechQueue_(nodes, {
      startCharIndex: offset,
    });
  }

  /**
   * Navigates to the next text block in the given direction.
   */
  private async navigateToNextParagraph_(direction: constants.Dir):
      Promise<void> {
    if (!this.isPaused_()) {
      // Stop TTS if it is currently playing.
      await this.pause_();
    }

    const nodes = NodeNavigationUtils.getNodesForNextParagraph(
        this.getCurrentNodeGroup_(), direction,
        nodes => this.skipPanel_(nodes));
    // Return early if the nodes are empty.
    if (nodes.length === 0) {
      return;
    }

    // Ensure the first node in the paragraph is visible.
    nodes[0].makeVisible();

    this.startSpeechQueue_(nodes);
  }

  /**
   * A predicate for paragraph selection and navigation. The current
   * implementation filters out paragraph that belongs to the panel.
   * @return Whether the paragraph made of the |nodes| is valid
   */
  private skipPanel_(nodes: AutomationNode[]): boolean {
    return !AutomationUtil.getAncestors(nodes[0]).find(
        n => UiManager.isPanel(n));
  }

  /**
   * Enqueue speech for the single given string. The string is not associated
   * with any particular nodes, so this does not do any work around drawing
   * focus rings, unlike startSpeechQueue_ below.
   * @param text The text to speak.
   */
  private startSpeech_(text: string): void {
    this.prepareForSpeech_(true /* clearFocusRing */);
    this.maybeShowEnhancedVoicesDialog_(() => {
      const options = this.prefsManager_.getSpeechOptions(null);
      const fallbackVoiceName = this.prefsManager_.getLocalVoice();

      // Without nodes to anchor on, navigate is not supported.
      this.supportsNavigationPanel_ = false;
      options.onEvent = event => {
        if (event.type === 'start') {
          this.onStateChanged_(SelectToSpeakState.SPEAKING);
          this.updateUi_();
        } else if (
            event.type === 'end' || event.type === 'interrupted' ||
            event.type === 'cancelled') {
          // Automatically dismiss when we're at the end.
          this.onStateChanged_(SelectToSpeakState.INACTIVE);
        }
      };
      const voiceName = options['voiceName'] || '';
      MetricsUtils.recordTtsEngineUsed(voiceName || '', this.prefsManager_);
      this.ttsManager_.speak(
          text, options, this.prefsManager_.isNetworkVoice(voiceName),
          fallbackVoiceName);
    });
  }

  /**
   * Enqueue nodes to TTS queue and start TTS. This function can be used for
   * adding nodes, either from user selection (e.g., mouse selection) or
   * navigation control (e.g., next paragraph).
   * @param  nodes The nodes to speak.
   * @param optParams:
   *    clearFocusRing: Whether to clear the focus ring or not. For example, we
   * need to clear the focus ring when starting from scratch but we do not need
   * to clear the focus ring when resuming from a previous pause. If this is not
   * passed, will default to false.
   *    startCharIndex: The index into the first node's text at which to start
   * speaking. If this is not passed, will start at 0.
   *    endCharIndex: The index into the last node's text at which to end
   * speech. If this is not passed, will stop at the end.
   */
  private startSpeechQueue_(nodes: AutomationNode[], optParams?: {
    clearFocusRing?: boolean,
    startCharIndex?: number,
    endCharIndex?: number,
  }): void {
    this.maybeShowEnhancedVoicesDialog_(() => {
      const params = optParams || {};
      const clearFocusRing = params.clearFocusRing || false;
      let startCharIndex = params.startCharIndex;
      let endCharIndex = params.endCharIndex;

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
    });
  }

  /**
   * Updates the node groups to be spoken. Converts |nodes|, |startCharIndex|,
   * and |endCharIndex| into node groups, and updates |this.currentNodeGroups_|
   * and |this.currentNodeGroupIndex_|.
   * @param nodes The nodes to speak.
   * @param startCharIndex The index into the first node's text at
   *     which to start speaking. If this is not passed, will start at 0.
   * @param endCharIndex The index into the last node's text at which
   *     to end speech. If this is not passed, will stop at the end.
   */
  private updateNodeGroups_(
      nodes: AutomationNode[], startCharIndex?: number,
      endCharIndex?: number): void {
    this.resetNodes_();

    for (let i = 0; i < nodes.length; i++) {
      // When navigation controls are enabled, disable the clipping of overflow
      // words. When overflow words are clipped, words scrolled out of view are
      // clipped, which is undesirable for our navigation features as we
      // generate node groups for next/previous paragraphs which may be fully or
      // partially scrolled out of view.
      const nodeGroup = ParagraphUtils.buildNodeGroup(nodes, i, {
        splitOnLanguage: this.shouldUseVoiceSwitching_(),
        clipOverflowWords: !this.shouldShowNavigationControls_(),
      });

      const isFirstNodeGroup = i === 0;
      const shouldApplyStartOffset =
          isFirstNodeGroup && startCharIndex !== undefined;
      const firstNodeHasInlineText =
          nodeGroup.nodes.length > 0 && nodeGroup.nodes[0].hasInlineText;
      if (shouldApplyStartOffset) {
        let startIndexInNodeGroup;
        if (firstNodeHasInlineText) {
          // We assume that the start offset will only be applied to the first
          // node in the first NodeGroup. The |startCharIndex| needs to be
          // adjusted. The first node of the NodeGroup may not be at the
          // beginning of the parent of the NodeGroup. (e.g., an inlineText in
          // its staticText parent). Thus, we need to adjust the start index.
          const startIndexInNodeParent =
              ParagraphUtils.getStartCharIndexInParent(nodes[0]);
          startIndexInNodeGroup = startCharIndex + startIndexInNodeParent +
              nodeGroup.nodes[0].startChar;
        } else {
          // Text field such as omnibox doesn't have inline text, but text in
          // the value property. In case the user selects some text within, we
          // need to adjust |startCharIndex| accordingly.
          startIndexInNodeGroup = startCharIndex + nodeGroup.nodes[0].startChar;
        }
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
      if (shouldApplyEndOffset) {
        let endIndexInNodeGroup;
        if (lastNodeHasInlineText) {
          // We assume that the end offset will only be applied to the last
          // node in the last NodeGroup. Similarly, |endCharIndex| needs to be
          // adjusted.
          const startIndexInNodeParent =
              ParagraphUtils.getStartCharIndexInParent(nodes[i]);
          endIndexInNodeGroup = endCharIndex + startIndexInNodeParent +
              nodeGroup.nodes[nodeGroup.nodes.length - 1].startChar;
        } else {
          // Text field such as omnibox doesn't have inline text, but text in
          // the value property. In case the user selects some text within, we
          // need to adjust |endCharIndex| accordingly.
          endIndexInNodeGroup = endCharIndex +
              nodeGroup.nodes[nodeGroup.nodes.length - 1].startChar;
        }
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
   */
  private startCurrentNodeGroup_(): void {
    const nodeGroup = this.getCurrentNodeGroup_();
    if (!nodeGroup) {
      return;
    }

    if (!nodeGroup.text) {
      this.onNodeGroupSpeakingCompleted_();
      return;
    }

    const options = this.getTtsOptionsForCurrentNodeGroup_();
    const voiceName = (options && options['voiceName']) || '';
    const fallbackVoiceName = this.prefsManager_.getLocalVoice();

    MetricsUtils.recordTtsEngineUsed(voiceName, this.prefsManager_);
    this.ttsManager_.speak(
        // TODO(b/314203187): Options may be undefined.
        nodeGroup.text, options!, this.prefsManager_.isNetworkVoice(voiceName),
        fallbackVoiceName);
  }

  private getTtsOptionsForCurrentNodeGroup_(): chrome.tts.TtsOptions|undefined {
    const nodeGroup = this.getCurrentNodeGroup_();
    if (!nodeGroup) {
      return;
    }
    const options: chrome.tts.TtsOptions = {};
    let language;
    let useVoiceSwitching = false;
    if (this.shouldUseVoiceSwitching_() && nodeGroup.detectedLanguage) {
      language = nodeGroup.detectedLanguage;
      useVoiceSwitching = true;
    }

    Object.assign(
        options,
        this.prefsManager_.getSpeechOptions({language, useVoiceSwitching}));

    if (this.shouldShowNavigationControls_()) {
      options.rate = this.getSpeechRate_();
      // Log speech rate multiple applied by Select-to-speak.
      MetricsUtils.recordSpeechRateOverrideMultiplier(
          this.speechRateMultiplier_);
    }

    const nodeGroupText = nodeGroup.text || '';

    options.onEvent = (event: chrome.tts.TtsEvent) => {
      switch (event.type) {
        case chrome.tts.EventType.START:
          if (nodeGroup.nodes.length <= 0) {
            break;
          }
          this.onStateChanged_(SelectToSpeakState.SPEAKING);

          // Update |this.currentCharIndex_|. Find the first non-space char
          // index in nodeGroup text, or 0 if the text is undefined or the first
          // char is non-space.
          this.currentCharIndex_ = nodeGroupText.search(/\S|$/);

          this.syncCurrentNodeWithCharIndex_(nodeGroup, this.currentCharIndex_);
          if (this.prefsManager_.wordHighlightingEnabled()) {
            // At start, find the first word and highlight that. Clear the
            // previous word in the node.
            this.currentNodeWord_ = null;
            // If |this.currentCharIndex_| is not 0, that means we have applied
            // a start offset. Thus, we need to pass startIndexInNodeGroup to
            // optStartIndex and overwrite the word boundaries in the original
            // node.
            this.updateNodeHighlight_(
                nodeGroupText, this.currentCharIndex_,
                this.currentCharIndex_ !== 0 ? this.currentCharIndex_ :
                                               undefined);
          } else {
            this.updateUi_();
          }
          break;
        case chrome.tts.EventType.RESUME:
          this.onTtsResumeSucceedEvent_(event);
          break;
        case chrome.tts.EventType.ERROR:
          if (event.errorMessage ===
              TtsManager.ErrorMessage.RESUME_WITH_EMPTY_CONTENT) {
            this.onTtsResumeErrorEvent_(event);
          }
          break;
        // @ts-expect-error: Fallthrough on purpose.
        case chrome.tts.EventType.PAUSE:
          // Updates the select to speak state to speaking to keep navigation
          // panel visible, so that the user can click resume from the panel.
          this.onStateChanged_(SelectToSpeakState.SPEAKING);
          // Fall through.
        case chrome.tts.EventType.INTERRUPTED:
        case chrome.tts.EventType.CANCELLED:
          if (!this.shouldShowNavigationControls_()) {
            this.onStateChanged_(SelectToSpeakState.INACTIVE);
            break;
          }
          if (this.state_ === SelectToSpeakState.SELECTING) {
            // Do not go into inactive state if navigation controls are enabled
            // and we're currently making a new selection. This enables users
            // to select new nodes while STS is active without first exiting.
            break;
          }
          break;
        case chrome.tts.EventType.END:
          this.onNodeGroupSpeakingCompleted_();
          break;
        case chrome.tts.EventType.WORD:
          this.onTtsWordEvent_(event, nodeGroup);
          break;
        default:
          break;
      }
    };

    return options;
  }

  /**
   * When a node group is completed, we start speaking the next node group
   * indicated by the end index. If we have reached the last node group, this
   * function will update STS status depending whether the navigation feature is
   * enabled.
   */
  private onNodeGroupSpeakingCompleted_(): void {
    const currentNodeGroup = this.getCurrentNodeGroup_();

    // Update the current char index to the end of the node group. If the
    // endOffset is undefined, we set the index to the length of the node
    // group's text.
    if (currentNodeGroup && currentNodeGroup.endOffset !== undefined) {
      this.currentCharIndex_ = currentNodeGroup.endOffset;
    } else {
      const nodeGroupText = (currentNodeGroup && currentNodeGroup.text) || '';
      this.currentCharIndex_ = nodeGroupText.length;
    }

    const isLastNodeGroup =
        (this.currentNodeGroupIndex_ === this.currentNodeGroups_.length - 1);
    if (isLastNodeGroup) {
      if (!this.shouldShowNavigationControls_()) {
        this.onStateChanged_(SelectToSpeakState.INACTIVE);
      } else {
        // If navigation features are enabled, we should keep STS state to
        // speaking so that the user can hit resume to continue.
        this.onStateChanged_(SelectToSpeakState.SPEAKING);
      }
      return;
    }

    // Start reading the next node group.
    this.currentNodeGroupIndex_++;
    this.startCurrentNodeGroup_();
  }

  /**
   * Update |this.currentNodeGroupItem_|, the current speaking or the node to be
   * spoken in the node group.
   * @param nodeGroup the current nodeGroup.
   * @param charIndex the start char index of the word to be spoken.
   *    The index is relative to the entire NodeGroup.
   * @param optStartFromNodeGroupIndex the NodeGroupIndex to start
   *    with. If undefined, search from 0.
   * @return If the found NodeGroupIndex is different from the
   *    |optStartFromNodeGroupIndex|.
   */
  private syncCurrentNodeWithCharIndex_(
      nodeGroup: ParagraphUtils.NodeGroup, charIndex: number,
      optStartFromNodeGroupIndex?: number): boolean {
    if (optStartFromNodeGroupIndex === undefined) {
      optStartFromNodeGroupIndex = 0;
    }

    // There is no speaking word, set the NodeGroupItemIndex to 0.
    if (charIndex <= 0) {
      this.currentNodeGroupItemIndex_ = 0;
      this.currentNodeGroupItem_ =
          nodeGroup.nodes[this.currentNodeGroupItemIndex_];
      return this.currentNodeGroupItemIndex_ === optStartFromNodeGroupIndex;
    }

    // Sets the |this.currentNodeGroupItemIndex_| to
    // |optStartFromNodeGroupIndex|
    this.currentNodeGroupItemIndex_ = optStartFromNodeGroupIndex;
    this.currentNodeGroupItem_ =
        nodeGroup.nodes[this.currentNodeGroupItemIndex_];

    if (this.currentNodeGroupItemIndex_ + 1 < nodeGroup.nodes.length) {
      let next: ParagraphUtils.NodeGroupItem|null =
          nodeGroup.nodes[this.currentNodeGroupItemIndex_ + 1];
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
   * @param nodeGroup the input nodeGroup.
   * @param offset the size of offset.
   * @param isStartOffset whether to apply a startOffset or an
   *     endOffset.
   */
  applyOffset(
      nodeGroup: ParagraphUtils.NodeGroup, offset: number,
      isStartOffset: boolean): void {
    if (isStartOffset) {
      // Applying start offset. Remove all text before the start index so that
      // it is not spoken. Backfill with spaces so that index counting
      // functions don't get confused.
      nodeGroup.text = ' '.repeat(offset) + nodeGroup.text.substr(offset);
    } else {
      // Remove all text after the end index so it is not spoken.
      nodeGroup.text = nodeGroup.text.substr(0, offset);
      nodeGroup.endOffset = offset;
    }
  }

  /**
   * Prepares for speech. Call once before this.ttsManager_.speak is called.
   * @param clearFocusRing Whether to clear the focus ring.
   */
  private prepareForSpeech_(clearFocusRing: boolean): void {
    this.cancelIfSpeaking_(clearFocusRing /* clear the focus ring */);

    // Update the UI on an interval, to adapt to automation tree changes.
    if (this.intervalId_ !== undefined) {
      clearInterval(this.intervalId_);
    }
    this.intervalId_ = setInterval(
        () => this.updateUi_(),
        SelectToSpeakConstants.NODE_STATE_TEST_INTERVAL_MS);
  }

  /**
   * Uses the 'word' speech event to determine which node is currently beings
   * spoken, and prepares for highlight if enabled.
   * @param event The event to use for updates.
   * @param nodeGroup The node group for this
   *     utterance.
   */
  private onTtsWordEvent_(
      event: chrome.tts.TtsEvent, nodeGroup: ParagraphUtils.NodeGroup): void {
    if (event.charIndex === undefined) {
      return;
    }
    // Not all speech engines include length in the ttsEvent object. .
    const hasLength = event.length !== undefined && event.length >= 0;
    const length = event.length || 0;
    // Only update the |this.currentCharIndex_| if event has a higher charIndex.
    // TTS sometimes will report an incorrect number at the end of an utterance.
    this.currentCharIndex_ = Math.max(event.charIndex, this.currentCharIndex_);
    console.debug(nodeGroup.text + ' (index ' + event.charIndex + ')');
    let debug = '-'.repeat(event.charIndex);
    if (hasLength) {
      debug += '^'.repeat(length);
    } else {
      debug += '^';
    }
    console.debug(debug);

    // First determine which node contains the word currently being spoken,
    // and update this.currentNodeGroupItem_, this.currentNodeWord_, and
    // this.currentNodeGroupItemIndex_ to match.
    const nodeUpdated = this.syncCurrentNodeWithCharIndex_(
        nodeGroup, event.charIndex, this.currentNodeGroupItemIndex_);
    if (nodeUpdated && !this.prefsManager_.wordHighlightingEnabled()) {
      // If we are doing a per-word highlight, we update the UI after figuring
      // out what the currently highlighted word is. Otherwise, update now.
      this.updateUi_();
    }

    // Finally update the word highlight if it is enabled.
    if (this.prefsManager_.wordHighlightingEnabled()) {
      if (hasLength) {
        this.currentNodeWord_ = {
          'start': event.charIndex - this.currentNodeGroupItem_!.startChar,
          'end':
              event.charIndex + length - this.currentNodeGroupItem_!.startChar,
        };
        this.updateUi_();
      } else {
        this.updateNodeHighlight_(nodeGroup.text, event.charIndex);
      }
    } else {
      this.currentNodeWord_ = null;
    }
  }

  /**
   * Updates the current node and relevant points to be the next node in the
   * group, then returns the next node in the group after that.
   */
  private incrementCurrentNodeAndGetNext_(nodeGroup: ParagraphUtils.NodeGroup):
      ParagraphUtils.NodeGroupItem|null {
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
   */
  private onStateChanged_(
      state: chrome.accessibilityPrivate.SelectToSpeakState): void {
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
   * @param clearFocusRing Whether to clear the focus ring as well.
   */
  private cancelIfSpeaking_(clearFocusRing: boolean): void {
    if (clearFocusRing) {
      this.stopAll_();
    } else {
      // Just stop speech
      this.ttsManager_.stop();
    }
  }

  /**
   * @return Promise that resolves to whether the given node
   *     should be considered in the foreground or not.
   */
  private isNodeInForeground_(node: AutomationNode): Promise<boolean> {
    return new Promise(resolve => {
      this.desktop_!.hitTestWithReply(
          node.location.left, node.location.top, nodeAtLocation => {
            chrome.automation.getFocus(focusedNode => {
              const window =
                  NodeUtils.getNearestContainingWindow(nodeAtLocation);
              const currentWindow = NodeUtils.getNearestContainingWindow(node);
              if (currentWindow != null && window != null &&
                  currentWindow === window) {
                resolve(true);
                return;
              }
              if (UiManager.isPanel(window) ||
                  UiManager.isPanel(
                      NodeUtils.getNearestContainingWindow(focusedNode))) {
                // If the focus is on the Select-to-speak panel or the hit test
                // landed on the panel, treat the current node as if it is in
                // the foreground.
                resolve(true);
                return;
              }
              if (focusedNode && currentWindow) {
                // See if the focused node window matches the currentWindow.
                // This may happen in some cases, for example, ARC++, when the
                // window which received the hit test request is not part of the
                // tree that contains the actual content. In such cases, use
                // focus to get the appropriate root.
                const focusedWindow =
                    NodeUtils.getNearestContainingWindow(focusedNode.root!);
                if (focusedWindow != null && currentWindow === focusedWindow) {
                  resolve(true);
                  return;
                }
              }
              resolve(false);
            });
          });
    });
  }

  /**
   * @return Current node that is being spoken.
   */
  private getCurrentSpokenNode_(): AutomationNode|null {
    if (!this.currentNodeGroupItem_) {
      return null;
    }
    if (this.currentNodeGroupItem_.hasInlineText && this.currentNodeWord_) {
      return ParagraphUtils.findInlineTextNodeByCharacterIndex(
          this.currentNodeGroupItem_.node, this.currentNodeWord_.start);
    } else if (
        this.currentNodeGroupItem_.hasInlineText &&
        this.shouldShowNavigationControls_()) {
      // If navigation controls are enabled, but word highlighting is disabled
      // (currentNodeWord_ === null), still find the inline text node so the
      // focus ring will highlight the whole block.
      return ParagraphUtils.findInlineTextNodeByCharacterIndex(
          this.currentNodeGroupItem_.node, 0);
    }
    // No inline text or word highlighting and navigation controls are
    // disabled.
    return this.currentNodeGroupItem_.node;
  }

  /**
   * Updates the UI based on the current STS and node state.
   * @return Promise that resolves when operation is complete.
   */
  private async updateUi_(): Promise<void> {
    if (this.currentNodeGroupItem_ === null) {
      // Nothing to do.
      return;
    }

    // Determine whether current node is in the foreground. If node has no
    // location, assume it is not in the foreground.
    const node = this.currentNodeGroupItem_.node;
    const inForeground = node.location !== undefined ?
        await this.isNodeInForeground_(node) :
        false;

    // Verify that current node item is still pointing to the same node after
    // asynchronous |isNodeInForeground_| operation.
    if (this.currentNodeGroupItem_ === null ||
        this.currentNodeGroupItem_.node !== node) {
      return;
    }

    const nodeState = NodeUtils.getNodeState(node);
    if (nodeState === NodeUtils.NodeState.NODE_STATE_INVALID ||
        nodeState === NodeUtils.NodeState.NODE_STATE_INVISIBLE ||
        !inForeground) {
      // Current node is in background or node is invalid/invisible.
      this.uiManager_.clear();
      return;
    }

    const spokenNode = this.getCurrentSpokenNode_();
    const currentNodeGroup = this.getCurrentNodeGroup_();
    if (!currentNodeGroup || !spokenNode) {
      console.warn('Could not update UI; no node group or spoken node');
      return;
    }

    if (this.scrollToSpokenNode_ && spokenNode.state!['offscreen']) {
      spokenNode.makeVisible();
    }
    const currentWord = this.prefsManager_.wordHighlightingEnabled() ?
        this.currentNodeWord_ :
        null;
    this.uiManager_.update(currentNodeGroup, spokenNode, currentWord, {
      showPanel: this.shouldShowNavigationControls_(),
      paused: this.isPaused_(),
      speechRateMultiplier: this.speechRateMultiplier_,
    });
  }

  /**
   * Shows a dialog to the user on first-run after enhanced voices update,
   * showing privacy disclaimer and asking if the user wants to turn on enhanced
   * network voices.
   *
   * @param callback Called back after user has confirmed or
   *     canceled in the dialog.
   */
  private maybeShowEnhancedVoicesDialog_(callback: () => any): void {
    if (!this.prefsManager_.enhancedVoicesDialogShown() &&
        this.prefsManager_.enhancedNetworkVoicesAllowed()) {
      // TODO(crbug.com/1230227): Style this dialog to match UX mocks.
      const title =
          chrome.i18n.getMessage('select_to_speak_natural_voice_dialog_title');
      const description = chrome.i18n.getMessage(
          'select_to_speak_natural_voice_dialog_description');
      const cancelName =
          chrome.i18n.getMessage('select_to_speak_natural_voice_dialog_cancel');
      chrome.accessibilityPrivate.showConfirmationDialog(
          title, description, cancelName, confirm => {
            this.prefsManager_.setEnhancedNetworkVoicesFromDialog(confirm);
            if (callback !== undefined) {
              callback();
            }
          });
    } else {
      // Flag not set or already shown, so we can continue the control flow
      // synchronously.
      if (callback !== undefined) {
        callback();
      }
    }
  }

  /**
   * Updates the currently highlighted node word based on the current text
   * and the character index of an event.
   * @param text The current text
   * @param charIndex The index of a current event in the text.
   * @param optStartIndex The index at which to start the
   *     highlight. This takes precedence over the charIndex.
   */
  private updateNodeHighlight_(
      text: string, charIndex: number, optStartIndex?: number): void {
    if (charIndex >= text.length) {
      // No need to do work if we are at the end of the paragraph.
      return;
    }
    // Get the next word based on the event's charIndex.
    const nextWordStart = WordUtils.getNextWordStart(
        text, charIndex, this.currentNodeGroupItem_!);
    // The |WordUtils.getNextWordEnd| will find the correct end based on the
    // trimmed text, so there is no need to provide additional input like
    // optStartIndex.
    const nextWordEnd = WordUtils.getNextWordEnd(
        text, optStartIndex === undefined ? nextWordStart : optStartIndex,
        this.currentNodeGroupItem_!);
    // Map the next word into the node's index from the text.
    const nodeStart = optStartIndex === undefined ?
        nextWordStart - this.currentNodeGroupItem_!.startChar :
        optStartIndex - this.currentNodeGroupItem_!.startChar;
    const nodeEnd = Math.min(
        nextWordEnd - this.currentNodeGroupItem_!.startChar,
        NodeUtils.nameLength(this.currentNodeGroupItem_!.node));
    if ((this.currentNodeWord_ == null ||
         nodeStart >= this.currentNodeWord_.end) &&
        nodeStart <= nodeEnd) {
      // Only update the bounds if they have increased from the
      // previous node. Because tts may send multiple callbacks
      // for the end of one word and the beginning of the next,
      // checking that the current word has changed allows us to
      // reduce extra work.
      this.currentNodeWord_ = {'start': nodeStart, 'end': nodeEnd};
      this.updateUi_();
    }
  }

  /**
   * @return Current speech rate.
   */
  private getSpeechRate_(): number {
    // Multiply default speech rate with user-selected multiplier.
    const rate = this.prefsManager_.speechRate() * this.speechRateMultiplier_;
    // Then round to the nearest tenth (ex. 1.799999 becomes 1.8).
    return Math.round(rate * 10) / 10;
  }

  /**
   * @return Whether all given nodes support the navigation panel.
   */
  private isNavigationPanelSupported_(nodes: AutomationNode[]): boolean {
    if (nodes.length === 0) {
      return true;
    }

    if (nodes.length === 1 && nodes[0] === nodes[0].root && nodes[0].parent &&
        nodes[0].parent.root &&
        nodes[0].parent.root.role === RoleType.DESKTOP) {
      // If the selected node is a root node within the desktop, such as a
      // a browser window, then do not show the navigation panel. There will
      // be no where for the user to navigate to. Also panel could be clipped
      // offscreen if the window is fullscreened.
      return false;
    }
    // Do not show panel on system UI. System UI can be problematic due to
    // auto-dismissing behavior (see http://crbug.com/1157148), but also
    // navigation controls do not work well for control-rich interfaces that are
    // light on text (and therefore no sentence and paragraph structures).
    return !nodes.some(n => n.root && n.root.role === RoleType.DESKTOP);
  }

  /**
   * @param keysPressed Which keys to pretend are currently pressed.
   */
  protected sendMockSelectToSpeakKeysPressedChanged(keysPressed: number[]):
      void {
    this.inputHandler_!.onKeysPressedChanged(new Set(keysPressed));
  }

  /**
   * Fires a mock mouse down event for testing.
   * @param type The event type.
   * @param mouseX The mouse x coordinate in global screen coordinates.
   * @param mouseY The mouse y coordinate in global screen coordinates.
   */
  protected fireMockMouseEvent(
      type: chrome.accessibilityPrivate.SyntheticMouseEventType, mouseX: number,
      mouseY: number): void {
    this.inputHandler_!.onMouseEvent(type, mouseX, mouseY);
  }

  /**
   * TODO(crbug.com/950391): Consider adding a metric for when voice switching
   * gets used.
   */
  private shouldUseVoiceSwitching_(): boolean {
    return this.prefsManager_.voiceSwitchingEnabled();
  }

  /**
   * Used by C++ tests to ensure STS load is completed.
   * @param callback Callback for when desktop is loaded from
   * automation.
   */
  setOnLoadDesktopCallbackForTest(callback: () => any): void {
    if (!this.desktop_) {
      this.onLoadDesktopCallbackForTest_ = callback;
      return;
    }
    // Desktop already loaded.
    callback();
  }
}

TestImportManager.exportForTesting(getGSuiteAppRoot);
