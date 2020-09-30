// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

goog.provide('Background');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('BackgroundKeyboardHandler');
goog.require('BrailleCommandData');
goog.require('BrailleCommandHandler');
goog.require('BrailleKeyCommand');
goog.require('ChromeVoxBackground');
goog.require('ChromeVoxEditableTextBase');
goog.require('ChromeVoxState');
goog.require('CommandHandler');
goog.require('DesktopAutomationHandler');
goog.require('DownloadHandler');
goog.require('ExtensionBridge');
goog.require('FindHandler');
goog.require('FocusAutomationHandler');
goog.require('GestureCommandHandler');
goog.require('InstanceChecker');
goog.require('LiveRegions');
goog.require('LocaleOutputHelper');
goog.require('MathHandler');
goog.require('MediaAutomationHandler');
goog.require('NavBraille');
goog.require('NextEarcons');
goog.require('NodeIdentifier');
goog.require('Notifications');
goog.require('Output');
goog.require('Output.EventType');
goog.require('PanelCommand');
goog.require('PhoneticData');
goog.require('RangeAutomationHandler');
goog.require('UserAnnotationHandler');
goog.require('constants');
goog.require('cursors.Cursor');


goog.scope(function() {
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/**
 * ChromeVox2 background page.
 */
Background = class extends ChromeVoxState {
  constructor() {
    super();

    // Initialize legacy background page first.
    ChromeVoxBackground.init();
    LocaleOutputHelper.init();

    /**
     * @type {cursors.Range}
     * @private
     */
    this.currentRange_ = null;

    /** @type {!AbstractEarcons} @private */
    this.nextEarcons_ = new NextEarcons();

    // Read-only earcons.
    Object.defineProperty(ChromeVox, 'earcons', {
      get: (function() {
             return this.nextEarcons_;
           }).bind(this)
    });

    Object.defineProperty(ChromeVox, 'modKeyStr', {
      get() {
        return 'Search';
      }
    });

    Object.defineProperty(ChromeVox, 'typingEcho', {
      get() {
        return parseInt(localStorage['typingEcho'], 10);
      },
      set(v) {
        localStorage['typingEcho'] = v;
      }
    });

    Object.defineProperty(ChromeVox, 'typingEcho', {
      get() {
        const typingEcho = parseInt(localStorage['typingEcho'], 10) || 0;
        return typingEcho;
      },
      set(value) {
        localStorage['typingEcho'] = value;
      }
    });

    ExtensionBridge.addMessageListener(this.onMessage_);

    /** @type {!BackgroundKeyboardHandler} @private */
    this.keyboardHandler_ = new BackgroundKeyboardHandler();

    /** @type {!LiveRegions} @private */
    this.liveRegions_ = new LiveRegions(this);

    document.addEventListener('copy', this.onClipboardEvent_);
    document.addEventListener('cut', this.onClipboardEvent_);
    document.addEventListener('paste', this.onClipboardEvent_);

    /** @private {boolean} */
    this.preventPasteOutput_ = false;

    /**
     * Maps a non-desktop root automation node to a range position suitable for
     *     restoration.
     * @type {WeakMap<AutomationNode, cursors.Range>}
     * @private
     */
    this.focusRecoveryMap_ = new WeakMap();

    /** @private {cursors.Range} */
    this.pageSel_;

    /** @type {boolean} */
    this.talkBackEnabled = false;

    // Initialize various handlers for automation.
    DesktopAutomationHandler.init();
    /** @private {!RangeAutomationHandler} */
    this.rangeAutomationHandler_ = new RangeAutomationHandler();
    /** @private {!FocusAutomationHandler} */
    this.focusAutomationHandler_ = new FocusAutomationHandler();
    /** @private {!MediaAutomationHandler} */
    this.mediaAutomationHandler_ = new MediaAutomationHandler();

    CommandHandler.init();
    FindHandler.init();
    DownloadHandler.init();
    PhoneticData.init();
    UserAnnotationHandler.init();

    Notifications.onStartup();

    chrome.accessibilityPrivate.onAnnounceForAccessibility.addListener(
        (announceText) => {
          ChromeVox.tts.speak(announceText.join(' '), QueueMode.FLUSH);
        });
    chrome.accessibilityPrivate.onCustomSpokenFeedbackToggled.addListener(
        (enabled) => {
          this.talkBackEnabled = enabled;
        });

    // Set the darkScreen state to false, since the display will be on whenever
    // ChromeVox starts.
    sessionStorage.setItem('darkScreen', 'false');

    // A self-contained class to start and stop progress sounds before any
    // speech has been generated on startup. This is important in cases where
    // speech is severely delayed.
    /** @implements {TtsCapturingEventListener} */
    const ProgressPlayer = class {
      constructor() {
        ChromeVox.tts.addCapturingEventListener(this);
        ChromeVox.earcons.playEarcon(Earcon.CHROMEVOX_LOADING);
      }

      /** @override */
      onTtsStart() {
        ChromeVox.earcons.playEarcon(Earcon.CHROMEVOX_LOADED);
        ChromeVox.tts.removeCapturingEventListener(this);
      }

      /** @override */
      onTtsEnd() {}
      /** @override */
      onTtsInterrupted() {}
    };
    new ProgressPlayer();

    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-chromevox-tutorial', (enabled) => {
          if (!enabled) {
            return;
          }

          chrome.loginState.getSessionState((sessionState) => {
            // If starting ChromeVox from OOBE, start the ChromeVox tutorial.
            // Use a timeout to allow ChromeVox to initialize first.
            if (sessionState ===
                chrome.loginState.SessionState.IN_OOBE_SCREEN) {
              setTimeout(() => {
                (new PanelCommand(PanelCommandType.TUTORIAL)).send();
              }, 1000);
            }
          });
        });
  }

  /**
   * Maps the last node with range in a given root.
   * @type {WeakMap<AutomationNode>}
   */
  get focusRecoveryMap() {
    return this.focusRecoveryMap_;
  }

  /**
   * @override
   */
  getCurrentRange() {
    if (this.currentRange_ && this.currentRange_.isValid()) {
      return this.currentRange_;
    }
    return null;
  }

  /**
   * @override
   */
  getCurrentRangeWithoutRecovery() {
    return this.currentRange_;
  }

  /**
   * @override
   */
  setCurrentRange(newRange) {
    // Clear anything that was frozen on the braille display whenever
    // the user navigates.
    ChromeVox.braille.thaw();

    if (newRange && !newRange.isValid()) {
      ChromeVoxState.instance.setFocusBounds([]);
      return;
    }

    this.currentRange_ = newRange;
    ChromeVoxState.observers.forEach(function(observer) {
      observer.onCurrentRangeChanged(newRange);
    });

    if (!this.currentRange_) {
      ChromeVoxState.instance.setFocusBounds([]);
      return;
    }

    const start = this.currentRange_.start.node;
    start.makeVisible();
    start.setAccessibilityFocus();

    const root = AutomationUtil.getTopLevelRoot(start);
    if (!root || root.role == RoleType.DESKTOP || root == start) {
      return;
    }

    const position = {};
    const loc = start.unclippedLocation;
    position.x = loc.left + loc.width / 2;
    position.y = loc.top + loc.height / 2;
    let url = root.docUrl;
    url = url.substring(0, url.indexOf('#')) || url;
    ChromeVox.position[url] = position;
  }

  /**
   * @override
   */
  navigateToRange(range, opt_focus, opt_speechProps, opt_shouldSetSelection) {
    opt_focus = opt_focus === undefined ? true : opt_focus;
    opt_speechProps = opt_speechProps || {};
    const prevRange = this.currentRange_;

    // Specialization for math output.
    let skipOutput = false;
    if (MathHandler.init(range)) {
      skipOutput = MathHandler.instance.speak();
      opt_focus = false;
    }

    if (opt_focus) {
      this.setFocusToRange_(range, prevRange);
    }

    this.setCurrentRange(range);

    const o = new Output();
    let selectedRange;
    let msg;

    if (this.pageSel_ && this.pageSel_.isValid() && range.isValid()) {
      // Suppress hints.
      o.withoutHints();

      // Selection across roots isn't supported.
      const pageRootStart = this.pageSel_.start.node.root;
      const pageRootEnd = this.pageSel_.end.node.root;
      const curRootStart = range.start.node.root;
      const curRootEnd = range.end.node.root;

      // Deny crossing over the start of the page selection and roots.
      if (pageRootStart != pageRootEnd || pageRootStart != curRootStart ||
          pageRootEnd != curRootEnd) {
        o.format('@end_selection');
        DesktopAutomationHandler.instance.ignoreDocumentSelectionFromAction(
            false);
        this.pageSel_ = null;
      } else {
        // Expand or shrink requires different feedback.

        // Page sel is the only place in ChromeVox where we used directed
        // selections. It is important to keep track of the directedness in
        // places, but when comparing to other ranges, take the undirected
        // range.
        const dir = this.pageSel_.normalize().compare(range);

        if (dir) {
          // Directed expansion.
          msg = '@selected';
        } else {
          // Directed shrink.
          msg = '@unselected';
          selectedRange = prevRange;
        }
        const wasBackwardSel =
            this.pageSel_.start.compare(this.pageSel_.end) == Dir.BACKWARD ||
            dir == Dir.BACKWARD;
        this.pageSel_ = new cursors.Range(
            this.pageSel_.start, wasBackwardSel ? range.start : range.end);
        if (this.pageSel_) {
          this.pageSel_.select();
        }
      }
    } else if (opt_shouldSetSelection) {
      // Ensure we don't select the editable when we first encounter it.
      let lca = null;
      if (range.start.node && prevRange.start.node) {
        lca = AutomationUtil.getLeastCommonAncestor(
            prevRange.start.node, range.start.node);
      }
      if (!lca || lca.state[StateType.EDITABLE] ||
          !range.start.node.state[StateType.EDITABLE]) {
        range.select();
      }
    }

    o.withRichSpeechAndBraille(
         selectedRange || range, prevRange, Output.EventType.NAVIGATE)
        .withQueueMode(QueueMode.FLUSH)
        .withInitialSpeechProperties(opt_speechProps);

    if (msg) {
      o.format(msg);
    }

    if (!skipOutput) {
      o.go();
    }
  }

  /**
   * Open the options page in a new tab.
   */
  showOptionsPage() {
    const optionsPage = {url: '/chromevox/options/options.html'};
    chrome.tabs.create(optionsPage);
  }

  /**
   * @override
   */
  onBrailleKeyEvent(evt, content) {
    return BrailleCommandHandler.onBrailleKeyEvent(evt, content);
  }

  /**
   * @param {Object} msg A message sent from a content script.
   * @param {Port} port
   * @private
   */
  onMessage_(msg, port) {
    const target = msg['target'];
    const action = msg['action'];

    switch (target) {
      case 'next':
        if (action == 'getIsClassicEnabled') {
          const url = msg['url'];
          const isClassicEnabled = false;
          port.postMessage({target: 'next', isClassicEnabled});
        } else if (action == 'onCommand') {
          CommandHandler.onCommand(msg['command']);
        } else if (action == 'flushNextUtterance') {
          Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
        }
        break;
    }
  }

  /**
   * @override
   */
  markCurrentRange() {
    if (!this.currentRange) {
      return;
    }

    const root = AutomationUtil.getTopLevelRoot(this.currentRange.start.node);
    if (root) {
      this.focusRecoveryMap_.set(root, this.currentRange);
    }
  }

  /**
   * Detects various clipboard events and provides spoken output.
   *
   * Note that paste is explicitly skipped sometimes because during a copy or
   * cut, the copied or cut text is retrieved by pasting into a fake text
   * area. To prevent this from triggering paste output, this staste is
   * tracked via a field.
   * @param {!Event} evt
   * @private
   */
  onClipboardEvent_(evt) {
    let text = '';
    if (evt.type == 'paste') {
      if (this.preventPasteOutput_) {
        this.preventPasteOutput_ = false;
        return;
      }
      text = evt.clipboardData.getData('text');
      ChromeVox.tts.speak(Msgs.getMsg(evt.type, [text]), QueueMode.QUEUE);
    } else if (evt.type == 'copy' || evt.type == 'cut') {
      this.preventPasteOutput_ = true;
      const textarea = document.createElement('textarea');
      document.body.appendChild(textarea);
      textarea.focus();
      document.execCommand('paste');
      const clipboardContent = textarea.value;
      textarea.remove();
      ChromeVox.tts.speak(
          Msgs.getMsg(evt.type, [clipboardContent]), QueueMode.FLUSH);
      ChromeVoxState.instance.pageSel_ = null;
    }
  }

  /** @private */
  setCurrentRangeToFocus_() {
    chrome.automation.getFocus(function(focus) {
      if (focus) {
        this.setCurrentRange(cursors.Range.fromNode(focus));
      } else {
        this.setCurrentRange(null);
      }
    }.bind(this));
  }

  /**
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @private
   */
  setFocusToRange_(range, prevRange) {
    const start = range.start.node;
    const end = range.end.node;

    // First, see if we've crossed a root. Remove once webview handles focus
    // correctly.
    if (prevRange && prevRange.start.node && start) {
      const entered =
          AutomationUtil.getUniqueAncestors(prevRange.start.node, start);

      entered
          .filter((f) => {
            return f.role == RoleType.PLUGIN_OBJECT ||
                f.role == RoleType.IFRAME;
          })
          .forEach((container) => {
            if (!container.state[StateType.FOCUSED]) {
              container.focus();
            }
          });
    }

    if (start.state[StateType.FOCUSED] || end.state[StateType.FOCUSED]) {
      return;
    }

    const isFocusableLinkOrControl = function(node) {
      return node.state[StateType.FOCUSABLE] &&
          AutomationPredicate.linkOrControl(node);
    };

    // Next, try to focus the start or end node.
    if (!AutomationPredicate.structuralContainer(start) &&
        start.state[StateType.FOCUSABLE]) {
      if (!start.state[StateType.FOCUSED]) {
        start.focus();
      }
      return;
    } else if (
        !AutomationPredicate.structuralContainer(end) &&
        end.state[StateType.FOCUSABLE]) {
      if (!end.state[StateType.FOCUSED]) {
        end.focus();
      }
      return;
    }

    // If a common ancestor of |start| and |end| is a link, focus that.
    let ancestor = AutomationUtil.getLeastCommonAncestor(start, end);
    while (ancestor && ancestor.root == start.root) {
      if (isFocusableLinkOrControl(ancestor)) {
        if (!ancestor.state[StateType.FOCUSED]) {
          ancestor.focus();
        }
        return;
      }
      ancestor = ancestor.parent;
    }

    // If nothing is focusable, set the sequential focus navigation starting
    // point, which ensures that the next time you press Tab, you'll reach
    // the next or previous focusable node from |start|.
    if (!start.state[StateType.OFFSCREEN]) {
      start.setSequentialFocusNavigationStartingPoint();
    }
  }

  /**
   * Converts a list of globs, as used in the extension manifest, to a regular
   * expression that matches if and only if any of the globs in the list
   * matches.
   * @param {!Array<string>} globs
   * @return {!RegExp}
   * @private
   */
  static globsToRegExp_(globs) {
    return new RegExp(
        '^(' +
        globs
            .map(function(glob) {
              return glob.replace(/[.+^$(){}|[\]\\]/g, '\\$&')
                  .replace(/\*/g, '.*')
                  .replace(/\?/g, '.');
            })
            .join('|') +
        ')$');
  }
};

InstanceChecker.closeExtraInstances();
new Background();

});  // goog.scope
