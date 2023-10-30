// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes that handle the ChromeVox range.
 */
import {AutomationPredicate} from '../../common/automation_predicate.js';
import {AutomationUtil} from '../../common/automation_util.js';
import {constants} from '../../common/constants.js';
import {CursorRange} from '../../common/cursors/range.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {TtsSpeechProperties} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import {ChromeVoxState} from './chromevox_state.js';
import {DesktopAutomationInterface} from './event/desktop_automation_interface.js';
import {FocusBounds} from './focus_bounds.js';
import {MathHandler} from './math_handler.js';
import {Output} from './output/output.js';
import {OutputCustomEvent} from './output/output_types.js';

const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;
const Action = BridgeConstants.ChromeVoxRange.Action;
const TARGET = BridgeConstants.ChromeVoxRange.TARGET;

/**
 * An interface implemented by objects to observe ChromeVox range changes.
 * @interface
 */
export class ChromeVoxRangeObserver {
  /**
   * @param {?CursorRange} range The new range.
   * @param {boolean=} opt_fromEditing
   */
  onCurrentRangeChanged(range, opt_fromEditing = undefined) {}
}

/**
 * A class that handles tracking and changes to the ChromeVox range.
 *
 * ================ THIS CLASS IS MID-MIGRATION ================
 *
 * The logic relating to the ChromeVox range is being moved here from
 * ChromeVoxState in small chunks. During this transition, the logic will be
 * split between those two locations.
 */
export class ChromeVoxRange {
  /** @private */
  constructor() {
    /** @private {?CursorRange} */
    this.current_ = null;
    /** @private {?CursorRange} */
    this.pageSel_ = null;
    /** @private {?CursorRange} */
    this.previous_ = null;
  }

  static init() {
    if (ChromeVoxRange.instance) {
      throw new Error('Cannot create more than one ChromeVoxRange');
    }
    ChromeVoxRange.instance = new ChromeVoxRange();

    BridgeHelper.registerHandler(
        TARGET, Action.CLEAR_CURRENT_RANGE, () => ChromeVoxRange.set(null));
  }

  /** @return {?CursorRange} */
  static get current() {
    if (ChromeVoxRange.instance.current_?.isValid()) {
      return ChromeVoxRange.instance.current_;
    }
    return null;
  }

  /** @return {?CursorRange} */
  static get pageSel() {
    return ChromeVoxRange.instance.pageSel_;
  }

  /** @param {?CursorRange} newPageSel */
  static set pageSel(newPageSel) {
    ChromeVoxRange.instance.pageSel_ = newPageSel;
  }

  /** @return {?CursorRange} */
  static get previous() {
    return ChromeVoxRange.instance.previous_;
  }

  /**
   * Return the current range, but focus recovery is not applied to it.
   * @return {?CursorRange}
   */
  static getCurrentRangeWithoutRecovery() {
    return ChromeVoxRange.instance.current_;
  }

  /**
   * Check for loss of focus which results in us invalidating our current range.
   */
  static maybeResetFromFocus() {
    ChromeVoxRange.instance.maybeResetFromFocus_();
  }

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!CursorRange} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {TtsSpeechProperties=} opt_speechProps Speech properties.
   * @param {boolean=} opt_skipSettingSelection If true, does not set
   *     the selection, otherwise it does by default.
   */
  static navigateTo(
      range, opt_focus, opt_speechProps, opt_skipSettingSelection) {
    ChromeVoxRange.instance.navigateTo_(...arguments);
  }

  /** Restores the last valid ChromeVox range. */
  static restoreLastValidRangeIfNeeded() {
    ChromeVoxRange.instance.restoreLastValidRangeIfNeeded_();
  }

  /**
   * @param {?CursorRange} newRange The new range.
   * @param {boolean=} opt_fromEditing
   */
  static set(newRange, opt_fromEditing) {
    ChromeVoxRange.instance.set_(...arguments);
  }

  // ================= Observer Functions =================

  /** @param {ChromeVoxRangeObserver} observer */
  static addObserver(observer) {
    ChromeVoxRange.observers_.push(observer);
  }

  /** @param {ChromeVoxRangeObserver} observer */
  static removeObserver(observer) {
    const index = ChromeVoxRange.observers_.indexOf(observer);
    if (index > -1) {
      ChromeVoxRange.observers_.splice(index, 1);
    }
  }

  // ================= Private Methods =================

  /**
   * Check for loss of focus which results in us invalidating our current
   * range. Note the getFocus() callback is synchronous, so the focus will be
   * updated when this function returns (despite being technicallly a separate
   * function call). Note: do not convert this method to async, as it would
   * change the execution order described above.
   * @private
   */
  maybeResetFromFocus_() {
    chrome.automation.getFocus(focus => {
      const cur = ChromeVoxRange.current;
      // If the current node is not valid and there's a current focus:
      if (cur && !cur.isValid() && focus) {
        ChromeVoxRange.set(CursorRange.fromNode(focus));
      }

      // If there's no focused node:
      if (!focus) {
        ChromeVoxRange.set(null);
        return;
      }

      // This case detects when TalkBack (in ARC++) is enabled (which also
      // covers when the ARC++ window is active). Clear the ChromeVox range
      // so keys get passed through for ChromeVox commands.
      if (ChromeVoxState.instance.talkBackEnabled &&
          // This additional check is not strictly necessary, but we use it to
          // ensure we are never inadvertently losing focus. ARC++ windows set
          // "focus" on a root view.
          focus.role === RoleType.CLIENT) {
        ChromeVoxRange.set(null);
      }
    });
  }

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!CursorRange} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {TtsSpeechProperties=} opt_speechProps Speech properties.
   * @param {boolean=} opt_skipSettingSelection If true, does not set
   *     the selection, otherwise it does by default.
   */
  navigateTo_(range, opt_focus, opt_speechProps, opt_skipSettingSelection) {
    opt_focus = opt_focus ?? true;
    opt_speechProps = opt_speechProps ?? new TtsSpeechProperties();
    opt_skipSettingSelection = opt_skipSettingSelection ?? false;
    const prevRange = ChromeVoxRange.getCurrentRangeWithoutRecovery();

    // Specialization for math output.
    let skipOutput = false;
    if (MathHandler.init(range)) {
      skipOutput = MathHandler.instance.speak();
      opt_focus = false;
    }

    if (opt_focus) {
      this.setFocusToRange_(range, prevRange);
    }

    ChromeVoxRange.set(range);

    const o = new Output();
    let selectedRange;
    let msg;

    if (this.pageSel_?.isValid() && range.isValid()) {
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
        DesktopAutomationInterface.instance.ignoreDocumentSelectionFromAction(
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
        this.pageSel_ = new CursorRange(
            this.pageSel_.start, wasBackwardSel ? range.start : range.end);
        this.pageSel_.select();
      }
    } else if (!opt_skipSettingSelection) {
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
         selectedRange ?? range, prevRange, OutputCustomEvent.NAVIGATE)
        .withInitialSpeechProperties(opt_speechProps);

    if (msg) {
      o.format(msg);
    }

    if (!skipOutput) {
      o.go();
    }
  }

  /**
   * @param {?CursorRange} range The new range.
   * @param {boolean=} opt_fromEditing
   * @private
   */
  notifyObservers_(range, opt_fromEditing = undefined) {
    for (const observer of ChromeVoxRange.observers_) {
      observer.onCurrentRangeChanged(range, opt_fromEditing);
    }
  }

  /** @private */
  restoreLastValidRangeIfNeeded_() {
    // Never restore range when TalkBack is enabled as commands such as
    // Search+Left, go directly to TalkBack.
    if (ChromeVoxState.instance.talkBackEnabled) {
      return;
    }

    if (!this.current_?.isValid()) {
      this.current_ = this.previous_;
    }
  }

  /**
   * @param {?CursorRange} newRange
   * @param {boolean=} opt_fromEditing
   * @private
   */
  set_(newRange, opt_fromEditing) {
    // Clear anything that was frozen on the braille display whenever
    // the user navigates.
    ChromeVox.braille.thaw();

    // There's nothing to be updated in this case.
    if ((!newRange && !this.current_) || (newRange && !newRange.isValid())) {
      FocusBounds.set([]);
      return;
    }

    this.previous_ = this.current_;
    this.current_ = newRange;

    this.notifyObservers_(newRange, opt_fromEditing);

    if (!this.current_) {
      FocusBounds.set([]);
      return;
    }

    const start = this.current_.start.node;
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
    ChromeVoxState.position[url] = position;
  }

  /**
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
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
          .filter(
              ancestor => ancestor.role === RoleType.PLUGIN_OBJECT ||
                  ancestor.role === RoleType.IFRAME)
          .forEach(container => {
            if (!container.state[StateType.FOCUSED]) {
              container.focus();
            }
          });
    }

    if (start.state[StateType.FOCUSED] || end.state[StateType.FOCUSED]) {
      return;
    }

    const isFocusableLinkOrControl = node => node.state[StateType.FOCUSABLE] &&
        AutomationPredicate.linkOrControl(node);

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
}

/** @private {!Array<ChromeVoxRangeObserver>} */
ChromeVoxRange.observers_ = [];

/** @type {ChromeVoxRange} */
ChromeVoxRange.instance;
