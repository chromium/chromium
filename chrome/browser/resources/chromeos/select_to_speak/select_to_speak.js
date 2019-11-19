// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = chrome.automation.AutomationEvent;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

// This must be the same as in ash/system/accessibility/select_to_speak_tray.cc:
// ash::kSelectToSpeakTrayClassName.
const SELECT_TO_SPEAK_TRAY_CLASS_NAME =
    'tray/TrayBackgroundView/SelectToSpeakTray';

// Matches one of the known GSuite apps which need the clipboard to find and
// read selected text. Includes sandbox and non-sandbox versions.
const GSUITE_APP_REGEXP =
    /^https:\/\/docs\.(?:sandbox\.)?google\.com\/(?:(?:presentation)|(?:document)|(?:spreadsheets)|(?:drawings)){1}\//;

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
function getGSuiteAppRoot(node) {
  while (node !== undefined && node.root !== undefined) {
    if (node.root.url !== undefined && GSUITE_APP_REGEXP.exec(node.root.url)) {
      return node.root;
    }
    node = node.root.parent;
  }
  return null;
}

/**
 * @constructor
 */
let SelectToSpeak = function() {
  /**
   * The current state of the SelectToSpeak extension, from
   * SelectToSpeakState.
   * @private {!chrome.accessibilityPrivate.SelectToSpeakState}
   */
  this.state_ = SelectToSpeakState.INACTIVE;

  /** @type {InputHandler} */
  this.inputHandler_ = null;

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
  }.bind(this));

  /** @private {boolean} */
  this.readAfterClose_ = true;

  /** @private {?ParagraphUtils.NodeGroupItem} */
  this.currentNode_ = null;

  /** @private {number} */
  this.currentNodeGroupIndex_ = -1;

  /**
   * The indexes within the current node representing the word currently being
   * spoken. Only updated if word highlighting is enabled.
   * @private {?Object}
   */
  this.currentNodeWord_ = null;

  /** @private {?AutomationNode} */
  this.currentBlockParent_ = null;

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
  this.null_selection_tone_ = new Audio('earcons/null_selection.ogg');

  /** @private {PrefsManager} */
  this.prefsManager_ = new PrefsManager();
  this.prefsManager_.initPreferences();

  this.runContentScripts_();
  this.setUpEventListeners_();

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
};

/** @const {number} */
SelectToSpeak.SEARCH_KEY_CODE = 91;

/** @const {number} */
SelectToSpeak.CONTROL_KEY_CODE = 17;

/** @const {number} */
SelectToSpeak.READ_SELECTION_KEY_CODE = 83;

/**
 * How often (in ms) to check that the currently spoken node is
 * still valid and in the same position. Decreasing this will make
 * STS seem more reactive to page changes but decreasing it too much
 * could cause performance issues.
 * @const {number}
 */
SelectToSpeak.NODE_STATE_TEST_INTERVAL_MS = 500;

SelectToSpeak.prototype = {
  /**
   * Called in response to our hit test after the mouse is released,
   * when the user is in a mode where select-to-speak is capturing
   * mouse events (for example holding down Search).
   * @param {!AutomationEvent} evt The automation event.
   * @private
   */
  onAutomationHitTest_: function(evt) {
    // Walk up to the nearest window, web area, toolbar, or dialog that the
    // hit node is contained inside. Only speak objects within that
    // container. In the future we might include other container-like
    // roles here.
    var root = evt.target;
    // TODO: Use AutomationPredicate.root instead?
    while (root.parent && root.role != RoleType.WINDOW &&
           root.role != RoleType.ROOT_WEB_AREA &&
           root.role != RoleType.DESKTOP && root.role != RoleType.DIALOG &&
           root.role != RoleType.ALERT_DIALOG &&
           root.role != RoleType.TOOLBAR) {
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
          focusedNode.root.role != RoleType.DESKTOP) {
        NodeUtils.findAllMatching(focusedNode.root, rect, nodes);
      }
      if (nodes.length == 1 &&
          AutomationUtil.getAncestors(nodes[0]).find(
              (n) => n.className == SELECT_TO_SPEAK_TRAY_CLASS_NAME)) {
        // Don't read only the Select-to-Speak toggle button in the tray unless
        // more items are being read.
        return;
      }
      this.startSpeechQueue_(nodes);
      MetricsUtils.recordStartEvent(
          MetricsUtils.StartSpeechMethod.MOUSE, this.prefsManager_);
    }.bind(this));
  },

  /**
   * Queues up selected text for reading by finding the Position objects
   * representing the selection.
   * @private
   */
  requestSpeakSelectedText_: function(focusedNode) {
    // If nothing is selected, return early.
    if (!focusedNode || !focusedNode.root ||
        !focusedNode.root.selectionStartObject ||
        !focusedNode.root.selectionEndObject) {
      this.onNullSelection_();
      return;
    }

    let startObject = focusedNode.root.selectionStartObject;
    let startOffset = focusedNode.root.selectionStartOffset || 0;
    let endObject = focusedNode.root.selectionEndObject;
    let endOffset = focusedNode.root.selectionEndOffset || 0;
    if (startObject === endObject && startOffset == endOffset) {
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

    let startPosition =
        NodeUtils.getDeepEquivalentForSelection(startObject, startOffset, true);
    let endPosition =
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
      let dir =
          AutomationUtil.getDirection(startPosition.node, endPosition.node);
      // Highlighting may be forwards or backwards. Make sure we start at the
      // first node.
      if (dir == constants.Dir.FORWARD) {
        firstPosition = startPosition;
        lastPosition = endPosition;
      } else {
        lastPosition = startPosition;
        firstPosition = endPosition;
      }
    }

    this.readNodesInSelection_(firstPosition, lastPosition, focusedNode);
  },

  /**
   * Reads nodes between the first and last position selected by the user.
   * @param {NodeUtils.Position} firstPosition The first position at which to
   *     start reading.
   * @param {NodeUtils.Position} lastPosition The last position at which to stop
   *     reading.
   * @param {AutomationNode} focusedNode The node with user focus.
   * @private
   */
  readNodesInSelection_: function(firstPosition, lastPosition, focusedNode) {
    let nodes = [];
    let selectedNode = firstPosition.node;
    if (selectedNode.name && firstPosition.offset < selectedNode.name.length &&
        !NodeUtils.shouldIgnoreNode(
            selectedNode, /* include offscreen */ true)) {
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
    while (selectedNode && selectedNode != lastPosition.node &&
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
              selectedNode, /* include offscreen */ true)) {
        nodes.push(selectedNode);
      }
    }
    if (nodes.length > 0) {
      if (lastPosition.node !== nodes[nodes.length - 1]) {
        // The node at the last position was not added to the list, perhaps it
        // was whitespace or invisible. Clear the ending offset because it
        // relates to a node that doesn't exist.
        this.startSpeechQueue_(nodes, firstPosition.offset);
      } else {
        this.startSpeechQueue_(
            nodes, firstPosition.offset, lastPosition.offset);
      }
      this.initializeScrollingToOffscreenNodes_(focusedNode.root);
      MetricsUtils.recordStartEvent(
          MetricsUtils.StartSpeechMethod.KEYSTROKE, this.prefsManager_);
    } else {
      let gsuiteAppRootNode = getGSuiteAppRoot(focusedNode);
      if (!gsuiteAppRootNode) {
        return;
      }
      chrome.tabs.query({active: true}, (tabs) => {
        // Closure doesn't realize that we did a !gsuiteAppRootNode earlier
        // so we check again here.
        if (tabs.length == 0 || !gsuiteAppRootNode) {
          return;
        }
        let tab = tabs[0];
        this.inputHandler_.onRequestReadClipboardData();
        this.currentNode_ =
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
  },

  /**
   * Gets ready to cancel future scrolling to offscreen nodes as soon as
   * a user-initiated scroll is done.
   * @param {AutomationNode=} root The root node to listen for events on.
   * @private
   */
  initializeScrollingToOffscreenNodes_: function(root) {
    if (!root) {
      return;
    }
    this.scrollToSpokenNode_ = true;
    let listener = (event) => {
      if (event.eventFrom != 'action') {
        // User initiated event. Cancel all future scrolling to spoken nodes.
        // If the user wants a certain scroll position we will respect that.
        this.scrollToSpokenNode_ = false;

        // Now remove this event listener, we no longer need it.
        root.removeEventListener(
            EventType.SCROLL_POSITION_CHANGED, listener, false);
      }
    };
    root.addEventListener(EventType.SCROLL_POSITION_CHANGED, listener, false);
  },

  /**
   * Plays a tone to let the user know they did the correct
   * keystroke but nothing was selected.
   * @private
   */
  onNullSelection_: function() {
    this.null_selection_tone_.play();
  },

  /**
   * Stop speech. If speech was in-progress, the interruption
   * event will be caught and clearFocusRingAndNode_ will be
   * called, stopping visual feedback as well.
   * If speech was not in progress, i.e. if the user was drawing
   * a focus ring on the screen, this still clears the visual
   * focus ring.
   * @private
   */
  stopAll_: function() {
    chrome.tts.stop();
    this.clearFocusRing_();
    this.onStateChanged_(SelectToSpeakState.INACTIVE);
  },

  /**
   * Clears the current focus ring and node, but does
   * not stop the speech.
   * @private
   */
  clearFocusRingAndNode_: function() {
    this.clearFocusRing_();
    // Clear the node and also stop the interval testing.
    this.currentNode_ = null;
    this.currentNodeGroupIndex_ = -1;
    this.currentNodeWord_ = null;
    clearInterval(this.intervalId_);
    this.intervalId_ = undefined;
    this.scrollToSpokenNode_ = false;
  },

  /**
   * Clears the focus ring, but does not clear the current
   * node.
   * @private
   */
  clearFocusRing_: function() {
    this.setFocusRings_([]);
    chrome.accessibilityPrivate.setHighlights(
        [], this.prefsManager_.highlightColor());
  },

  /**
   * Sets the focus ring to |rects|.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @private
   */
  setFocusRings_: function(rects) {
    chrome.accessibilityPrivate.setFocusRings([{
      rects: rects,
      type: chrome.accessibilityPrivate.FocusType.GLOW,
      color: this.prefsManager_.focusRingColor()
    }]);
  },

  /**
   * Runs content scripts that allow Select-to-Speak access to
   * Google Docs content without a11y mode enabled, in every open
   * tab. Should be run when Select-to-Speak starts up so that any
   * tabs already opened will be checked.
   * This should be kept in sync with the "content_scripts" section in
   * the Select-to-Speak manifest.
   * @private
   */
  runContentScripts_: function() {
    chrome.tabs.query(
        {
          url: [
            'https://docs.google.com/document*',
            'https://docs.sandbox.google.com/*'
          ]
        },
        (tabs) => {
          tabs.forEach((tab) => {
            chrome.tabs.executeScript(
                tab.id, {file: 'select_to_speak_gdocs_script.js'});
          });
        });
  },

  /**
   * Set up event listeners user input.
   * @private
   */
  setUpEventListeners_: function() {
    this.inputHandler_ = new InputHandler({
      // canStartSelecting: Whether mouse selection can begin.
      canStartSelecting: () => {
        return this.state_ != SelectToSpeakState.SELECTING;
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
        this.setFocusRings_([rect]);
      },
      // onKeystrokeSelection: Keys pressed for reading highlighted text.
      onKeystrokeSelection: () => {
        this.cancelIfSpeaking_(true /* clear the focus ring */);
        chrome.automation.getFocus(this.requestSpeakSelectedText_.bind(this));
      },
      // onRequestCancel: User requested canceling input/speech.
      onRequestCancel: () => {
        this.cancelIfSpeaking_(true /* clear the focus ring */);
      },
      // onTextReceived: Text received from a 'paste' event to read aloud.
      onTextReceived: this.startSpeech_.bind(this)
    });
    this.inputHandler_.setUpEventListeners();
    chrome.accessibilityPrivate.onSelectToSpeakStateChangeRequested.addListener(
        this.onStateChangeRequested_.bind(this));
    // Initialize the state to SelectToSpeakState.INACTIVE.
    chrome.accessibilityPrivate.onSelectToSpeakStateChanged(this.state_);
  },

  /**
   * Called when Chrome OS is requesting Select-to-Speak to switch states.
   */
  onStateChangeRequested_: function() {
    // Switch Select-to-Speak states on request.
    // We will need to track the current state and toggle from one state to
    // the next when this function is called, and then call
    // accessibilityPrivate.onSelectToSpeakStateChanged with the new state.
    switch (this.state_) {
      case SelectToSpeakState.INACTIVE:
        // Start selection.
        this.inputHandler_.setTrackingMouse(true);
        this.onStateChanged_(SelectToSpeakState.SELECTING);
        MetricsUtils.recordSelectToSpeakStateChangeEvent(
            MetricsUtils.StateChangeEvent.START_SELECTION);
        break;
      case SelectToSpeakState.SPEAKING:
        // Stop speaking.
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
  },

  /**
   * Enqueue speech for the single given string. The string is not associated
   * with any particular nodes, so this does not do any work around drawing
   * focus rings, unlike startSpeechQueue_ below.
   * @param {string} text The text to speak.
   * @private
   */
  startSpeech_: function(text) {
    this.prepareForSpeech_();
    let options = this.prefsManager_.speechOptions();
    options.onEvent = (event) => {
      if (event.type == 'start') {
        this.onStateChanged_(SelectToSpeakState.SPEAKING);
        this.testCurrentNode_();
      } else if (
          event.type == 'end' || event.type == 'interrupted' ||
          event.type == 'cancelled') {
        this.onStateChanged_(SelectToSpeakState.INACTIVE);
      }
    };
    chrome.tts.speak(text, options);
  },

  /**
   * Enqueue speech commands for all of the given nodes.
   * @param {Array<AutomationNode>} nodes The nodes to speak.
   * @param {number=} opt_startIndex The index into the first node's text
   * at which to start speaking. If this is not passed, will start at 0.
   * @param {number=} opt_endIndex The index into the last node's text
   * at which to end speech. If this is not passed, will stop at the end.
   * @private
   */
  startSpeechQueue_: function(nodes, opt_startIndex, opt_endIndex) {
    this.prepareForSpeech_();
    for (var i = 0; i < nodes.length; i++) {
      let nodeGroup = ParagraphUtils.buildNodeGroup(
          nodes, i, this.enableLanguageDetectionIntegration_);

      if (i == 0) {
        // We need to start in the middle of a node. Remove all text before
        // the start index so that it is not spoken.
        // Backfill with spaces so that index counting functions don't get
        // confused.
        // Must check opt_startIndex in its own if statement to make the
        // Closure compiler happy.
        if (opt_startIndex !== undefined) {
          if (nodeGroup.nodes.length > 0 && nodeGroup.nodes[0].hasInlineText) {
            // The first node is inlineText type. Find the start index in
            // its staticText parent.
            let startIndexInParent =
                ParagraphUtils.getStartCharIndexInParent(nodes[0]);
            opt_startIndex += startIndexInParent;
            nodeGroup.text = ' '.repeat(opt_startIndex) +
                nodeGroup.text.substr(opt_startIndex);
          }
        }
      }
      let isFirst = i == 0;
      // Advance i to the end of this group, to skip all nodes it contains.
      i = nodeGroup.endIndex;
      let isLast = (i == nodes.length - 1);
      if (isLast && opt_endIndex !== undefined && nodeGroup.nodes.length > 0) {
        // We need to stop in the middle of a node. Remove all text after
        // the end index so it is not spoken. Backfill with spaces so that
        // index counting functions don't get confused.
        // This only applies to inlineText nodes.
        if (nodeGroup.nodes[nodeGroup.nodes.length - 1].hasInlineText) {
          let startIndexInParent =
              ParagraphUtils.getStartCharIndexInParent(nodes[i]);
          opt_endIndex += startIndexInParent;
          nodeGroup.text = nodeGroup.text.substr(
              0,
              nodeGroup.nodes[nodeGroup.nodes.length - 1].startChar +
                  opt_endIndex);
        }
      }
      if (nodeGroup.nodes.length == 0 && !isLast) {
        continue;
      }

      let options = {};
      /* Copy options so we can add lang below */
      Object.assign(options, this.prefsManager_.speechOptions());
      if (this.enableLanguageDetectionIntegration_ &&
          nodeGroup.detectedLanguage) {
        options.lang = nodeGroup.detectedLanguage;
      }

      options.onEvent = (event) => {
        if (event.type == 'start' && nodeGroup.nodes.length > 0) {
          this.onStateChanged_(SelectToSpeakState.SPEAKING);
          this.currentBlockParent_ = nodeGroup.blockParent;
          this.currentNodeGroupIndex_ = 0;
          this.currentNode_ = nodeGroup.nodes[this.currentNodeGroupIndex_];
          if (this.prefsManager_.wordHighlightingEnabled()) {
            // At 'start', find the first word and highlight that.
            // Clear the previous word in the node.
            this.currentNodeWord_ = null;
            // If this is the first nodeGroup, pass the opt_startIndex.
            // If this is the last nodeGroup, pass the opt_endIndex.
            this.updateNodeHighlight_(
                nodeGroup.text, event.charIndex,
                isFirst ? opt_startIndex : undefined,
                isLast ? opt_endIndex : undefined);
          } else {
            this.testCurrentNode_();
          }
        } else if (event.type == 'interrupted' || event.type == 'cancelled') {
          this.onStateChanged_(SelectToSpeakState.INACTIVE);
        } else if (event.type == 'end') {
          if (isLast) {
            this.onStateChanged_(SelectToSpeakState.INACTIVE);
          }
        } else if (event.type == 'word') {
          this.onTtsWordEvent_(
              event, nodeGroup, isLast ? opt_endIndex : undefined);
        }
      };
      chrome.tts.speak(nodeGroup.text || '', options);
    }
  },

  /**
   * Prepares for speech. Call once before chrome.tts.speak is called.
   * @private
   */
  prepareForSpeech_: function() {
    this.cancelIfSpeaking_(true /* clear the focus ring */);
    if (this.intervalRef_ !== undefined) {
      clearInterval(this.intervalRef_);
    }
    this.intervalRef_ = setInterval(
        this.testCurrentNode_.bind(this),
        SelectToSpeak.NODE_STATE_TEST_INTERVAL_MS);
  },

  /**
   * Uses the 'word' speech event to determine which node is currently beings
   * spoken, and prepares for highlight if enabled.
   * @param {!TtsEvent} event The event to use for updates.
   * @param {ParagraphUtils.NodeGroup} nodeGroup The node group for this
   *     utterance.
   * @param {number=} opt_endIndex The last index for speech, if applicable.
   * @private
   */
  onTtsWordEvent_: function(event, nodeGroup, opt_endIndex) {
    // Not all speech engines include length in the ttsEvent object. If the
    // engine does have it, it makes word highlighting easier and more
    // accurate.
    let hasLength = event.length !== undefined && event.length >= 0;
    console.debug(nodeGroup.text + ' (index ' + event.charIndex + ')');
    let debug = '-'.repeat(event.charIndex);
    if (hasLength) {
      debug += '^'.repeat(event.length);
    } else {
      debug += '^';
    }
    console.debug(debug);

    // First determine which node contains the word currently being spoken,
    // and update this.currentNode_, this.currentNodeWord_, and
    // this.currentNodeGroupIndex_ to match.
    if (this.currentNodeGroupIndex_ + 1 < nodeGroup.nodes.length) {
      let next = nodeGroup.nodes[this.currentNodeGroupIndex_ + 1];
      let nodeUpdated = false;
      // TODO(katie): For something like a date, the start and end
      // node group nodes can actually be different. Example:
      // "<span>Tuesday,</span> December 18, 2018".
      if (hasLength) {
        while (next && event.charIndex >= next.startChar &&
               this.currentNodeGroupIndex_ + 1 < nodeGroup.nodes.length) {
          next = this.incrementCurrentNodeAndGetNext_(nodeGroup);
          nodeUpdated = true;
        }

        // Check if we've reached this next node yet using the
        // character index of the event. Add 1 for the space character
        // between node names, and another to make it to the start of the
        // next node name.
        // TODO: Do not use next.name.length instead use the next-next startChar
        while (next &&
               event.charIndex + event.length + 2 >=
                   next.startChar + next.node.name.length &&
               this.currentNodeGroupIndex_ + 1 < nodeGroup.nodes.length) {
          next = this.incrementCurrentNodeAndGetNext_(nodeGroup);
          nodeUpdated = true;
        }
      } else {
        while (next && event.charIndex + 2 >= next.startChar &&
               this.currentNodeGroupIndex_ + 1 < nodeGroup.nodes.length) {
          next = this.incrementCurrentNodeAndGetNext_(nodeGroup);
          nodeUpdated = true;
        }
      }
      if (nodeUpdated) {
        if (!this.prefsManager_.wordHighlightingEnabled()) {
          // If we are doing a per-word highlight, we will test the
          // node after figuring out what the currently highlighted
          // word is. Otherwise, test it now.
          this.testCurrentNode_();
        }
      }
    }

    // Finally update the word highlight if it is enabled.
    if (this.prefsManager_.wordHighlightingEnabled()) {
      if (hasLength) {
        this.currentNodeWord_ = {
          'start': event.charIndex - this.currentNode_.startChar,
          'end': event.charIndex + event.length - this.currentNode_.startChar
        };
        this.testCurrentNode_();
      } else {
        this.updateNodeHighlight_(
            nodeGroup.text, event.charIndex, undefined, opt_endIndex);
      }
    } else {
      this.currentNodeWord_ = null;
    }
  },

  /**
   * Updates the current node and relevant points to be the next node in the
   * group, then returns the next node in the group after that.
   * @param {!ParagraphUtils.NodeGroup} nodeGroup
   * @return {ParagraphUtils.NodeGroupItem}
   * @private
   */
  incrementCurrentNodeAndGetNext_: function(nodeGroup) {
    // Move to the next node.
    this.currentNodeGroupIndex_ += 1;
    this.currentNode_ = nodeGroup.nodes[this.currentNodeGroupIndex_];
    // Setting this.currentNodeWord_ to null signals it should be recalculated
    // later.
    this.currentNodeWord_ = null;
    if (this.currentNodeGroupIndex_ + 1 >= nodeGroup.nodes.length) {
      return null;
    }
    return nodeGroup.nodes[this.currentNodeGroupIndex_ + 1];
  },

  /**
   * Updates the state.
   * @param {!chrome.accessibilityPrivate.SelectToSpeakState} state
   * @private
   */
  onStateChanged_: function(state) {
    if (this.state_ != state) {
      if (this.state_ == SelectToSpeakState.SELECTING &&
          state == SelectToSpeakState.INACTIVE && this.trackingMouse_) {
        // If we are tracking the mouse actively, then we have requested tts
        // to stop speaking just before mouse tracking began, so we
        // shouldn't transition into the inactive state now: The call to stop
        // speaking created an async 'cancel' event from the TTS engine that
        // is now resulting in an attempt to set the state inactive.
        return;
      }
      if (state == SelectToSpeakState.INACTIVE) {
        this.clearFocusRingAndNode_();
      }
      // Send state change event to Chrome.
      chrome.accessibilityPrivate.onSelectToSpeakStateChanged(state);
      this.state_ = state;
    }
  },

  /**
   * Cancels the current speech queue after doing a callback to
   * record a cancel event if speech was in progress. We must cancel
   * before the callback (rather than in it) to avoid race conditions
   * where cancel is called twice.
   * @param {boolean} clearFocusRing Whether to clear the focus ring
   *    as well.
   * @private
   */
  cancelIfSpeaking_: function(clearFocusRing) {
    chrome.tts.isSpeaking(MetricsUtils.recordCancelIfSpeaking);
    if (clearFocusRing) {
      this.stopAll_();
    } else {
      // Just stop speech
      chrome.tts.stop();
    }
  },

  /**
   * Hides the speech and focus ring states if necessary based on a node's
   * current state.
   *
   * @param {ParagraphUtils.NodeGroupItem} nodeGroupItem The node to use for
   *     updates.
   * @param {boolean} inForeground Whether the node is in the foreground window.
   * @private
   */
  updateFromNodeState_: function(nodeGroupItem, inForeground) {
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
  },

  /**
   * Updates the speech and focus ring states based on a node's current state.
   *
   * @param {ParagraphUtils.NodeGroupItem} nodeGroupItem The node to use for
   *    updates.
   * @private
   */
  updateHighlightAndFocus_: function(nodeGroupItem) {
    if (!this.visible_) {
      return;
    }
    let node = nodeGroupItem.hasInlineText && this.currentNodeWord_ ?
        ParagraphUtils.findInlineTextNodeByCharacterIndex(
            nodeGroupItem.node, this.currentNodeWord_.start) :
        nodeGroupItem.node;
    if (this.scrollToSpokenNode_ && node.state.offscreen) {
      node.makeVisible();
    }
    if (this.prefsManager_.wordHighlightingEnabled() &&
        this.currentNodeWord_ != null) {
      var charIndexInParent = 0;
      // getStartCharIndexInParent is only defined for nodes with role
      // INLINE_TEXT_BOX.
      if (node.role == RoleType.INLINE_TEXT_BOX) {
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
    if (this.currentBlockParent_ != null &&
        node.role == RoleType.INLINE_TEXT_BOX) {
      this.setFocusRings_([this.currentBlockParent_.location]);
    } else {
      this.setFocusRings_([node.location]);
    }
  },

  /**
   * Tests the active node to make sure the bounds are drawn correctly.
   * @private
   */
  testCurrentNode_: function() {
    if (this.currentNode_ == null) {
      return;
    }
    if (this.currentNode_.node.location === undefined) {
      // Don't do the hit test because there is no location to test against.
      // Just directly update Select To Speak from node state.
      this.updateFromNodeState_(this.currentNode_, false);
    } else {
      this.updateHighlightAndFocus_(this.currentNode_);
      // Do a hit test to make sure the node is not in a background window
      // or minimimized. On the result checkCurrentNodeMatchesHitTest_ will be
      // called, and we will use that result plus the currentNode's state to
      // determine how to set the focus and whether to stop speech.
      this.desktop_.hitTest(
          this.currentNode_.node.location.left,
          this.currentNode_.node.location.top, EventType.HOVER);
    }
  },

  /**
   * Checks that the current node is in the same window as the HitTest node.
   * Uses this information to update Select-To-Speak from node state.
   * @private
   */
  onHitTestCheckCurrentNodeMatches_: function(evt) {
    if (this.currentNode_ == null) {
      return;
    }
    chrome.automation.getFocus(function(focusedNode) {
      var window = NodeUtils.getNearestContainingWindow(evt.target);
      var currentWindow =
          NodeUtils.getNearestContainingWindow(this.currentNode_.node);
      var inForeground =
          currentWindow != null && window != null && currentWindow == window;
      if (!inForeground && focusedNode && currentWindow) {
        // See if the focused node window matches the currentWindow.
        // This may happen in some cases, for example, ARC++, when the window
        // which received the hit test request is not part of the tree that
        // contains the actual content. In such cases, use focus to get the
        // appropriate root.
        var focusedWindow =
            NodeUtils.getNearestContainingWindow(focusedNode.root);
        inForeground = focusedWindow != null && currentWindow == focusedWindow;
      }
      this.updateFromNodeState_(this.currentNode_, inForeground);
    }.bind(this));
  },

  /**
   * Updates the currently highlighted node word based on the current text
   * and the character index of an event.
   * @param {string} text The current text
   * @param {number} charIndex The index of a current event in the text.
   * @param {number=} opt_startIndex The index at which to start the highlight.
   * This takes precedence over the charIndex.
   * @param {number=} opt_endIndex The index at which to end the highlight. This
   * takes precedence over the next word end.
   * @private
   */
  updateNodeHighlight_: function(
      text, charIndex, opt_startIndex, opt_endIndex) {
    if (charIndex >= text.length) {
      // No need to do work if we are at the end of the paragraph.
      return;
    }
    // Get the next word based on the event's charIndex.
    let nextWordStart =
        WordUtils.getNextWordStart(text, charIndex, this.currentNode_);
    let nextWordEnd = WordUtils.getNextWordEnd(
        text, opt_startIndex === undefined ? nextWordStart : opt_startIndex,
        this.currentNode_);
    // Map the next word into the node's index from the text.
    let nodeStart = opt_startIndex === undefined ?
        nextWordStart - this.currentNode_.startChar :
        opt_startIndex - this.currentNode_.startChar;
    let nodeEnd = Math.min(
        nextWordEnd - this.currentNode_.startChar,
        NodeUtils.nameLength(this.currentNode_.node));
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
  },

  // ---------- Functionality for testing ---------- //

  /**
   * Fires a mock key down event for testing.
   * @param {!Event} event The fake key down event to fire. The object
   * must contain at minimum a keyCode.
   * @protected
   */
  fireMockKeyDownEvent: function(event) {
    this.inputHandler_.onKeyDown_(event);
  },

  /**
   * Fires a mock key up event for testing.
   * @param {!Event} event The fake key up event to fire. The object
   * must contain at minimum a keyCode.
   * @protected
   */
  fireMockKeyUpEvent: function(event) {
    this.inputHandler_.onKeyUp_(event);
  },

  /**
   * Fires a mock mouse down event for testing.
   * @param {!Event} event The fake mouse down event to fire. The object
   * must contain at minimum a screenX and a screenY.
   * @protected
   */
  fireMockMouseDownEvent: function(event) {
    this.inputHandler_.onMouseDown_(event);
  },

  /**
   * Fires a mock mouse up event for testing.
   * @param {!Event} event The fake mouse up event to fire. The object
   * must contain at minimum a screenX and a screenY.
   * @protected
   */
  fireMockMouseUpEvent: function(event) {
    this.inputHandler_.onMouseUp_(event);
  },

  /**
   * Function to be called when a state change request is received from the
   * accessibilityPrivate API.
   * @type {?function()}
   * @protected
   */
  onStateChangeRequestedCallbackForTest_: null,
};
