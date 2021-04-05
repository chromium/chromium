// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Earcons} from './earcons.js';
import {FindHandler} from './find_handler.js';
import {LiveRegions} from './live_regions.js';
import {MediaAutomationHandler} from './media_automation_handler.js';
import {RangeAutomationHandler} from './range_automation_handler.js';

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/**
 * ChromeVox2 background page.
 */
export class Background extends ChromeVoxState {
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
    this.earcons_ = new Earcons();

    // Read-only earcons.
    Object.defineProperty(ChromeVox, 'earcons', {
      get: (function() {
             return this.earcons_;
           }).bind(this)
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

    /** @private {string|undefined} */
    this.lastClipboardEvent_;

    chrome.clipboard.onClipboardDataChanged.addListener(
        this.onClipboardDataChanged_.bind(this));
    document.addEventListener('copy', this.onClipboardCopyEvent_.bind(this));

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

    chrome.loginState.getSessionState((sessionState) => {
      if (sessionState === chrome.loginState.SessionState.IN_OOBE_SCREEN) {
        chrome.chromeosInfoPrivate.get(['deviceType'], (result) => {
          if (result['deviceType'] ===
              chrome.chromeosInfoPrivate.DeviceType.CHROMEBOOK) {
            // Start the tutorial if the following are true:
            // 1. We are in the OOBE.
            // 2. The device is a Chromebook.
            (new PanelCommand(PanelCommandType.TUTORIAL)).send();
          }
        });
      }
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
    if (!root || root.role === RoleType.DESKTOP || root === start) {
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
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!cursors.Range} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {Object=} opt_speechProps Speech properties.
   * @param {boolean=} opt_shouldSetSelection If true, does set
   *     the selection.
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
      if (pageRootStart !== pageRootEnd || pageRootStart !== curRootStart ||
          pageRootEnd !== curRootEnd) {
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
            this.pageSel_.start.compare(this.pageSel_.end) === Dir.BACKWARD ||
            dir === Dir.BACKWARD;
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
        if (action === 'getIsClassicEnabled') {
          const url = msg['url'];
          const isClassicEnabled = false;
          port.postMessage({target: 'next', isClassicEnabled});
        } else if (action === 'onCommand') {
          CommandHandler.onCommand(msg['command']);
        } else if (action === 'flushNextUtterance') {
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

  /** @override */
  readNextClipboardDataChange() {
    this.lastClipboardEvent_ = 'copy';
  }

  /**
   * Processes the copy clipboard event.
   * @param {!Event} evt
   * @private
   */
  onClipboardCopyEvent_(evt) {
    // This should always be 'copy', but is still important to set for the below
    // extension event.
    this.lastClipboardEvent_ = evt.type;
  }

  /** @private */
  onClipboardDataChanged_() {
    // A DOM-based clipboard event always comes before this Chrome extension
    // clipboard event. We only care about 'copy' events, which gets set above.
    if (!this.lastClipboardEvent_) {
      return;
    }

    const eventType = this.lastClipboardEvent_;
    this.lastClipboardEvent_ = undefined;

    const textarea = document.createElement('textarea');
    document.body.appendChild(textarea);
    textarea.focus();
    document.execCommand('paste');
    const clipboardContent = textarea.value;
    textarea.remove();
    ChromeVox.tts.speak(
        Msgs.getMsg(eventType, [clipboardContent]), QueueMode.FLUSH);
    ChromeVoxState.instance.pageSel_ = null;
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
            return f.role === RoleType.PLUGIN_OBJECT ||
                f.role === RoleType.IFRAME;
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
    while (ancestor && ancestor.root === start.root) {
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
}

InstanceChecker.closeExtraInstances();
new Background();
