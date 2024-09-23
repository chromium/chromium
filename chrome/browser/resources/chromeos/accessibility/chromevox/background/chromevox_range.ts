// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes that handle the ChromeVox range.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {BridgeHelper} from '/common/bridge_helper.js';
import {constants} from '/common/constants.js';
import {Cursor} from '/common/cursors/cursor.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from '../common/bridge_constants.js';
import {EarconId} from '../common/earcon_id.js';
import {TtsSpeechProperties} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import {ChromeVoxState} from './chromevox_state.js';
import {DesktopAutomationInterface} from './event/desktop_automation_interface.js';
import {FocusBounds} from './focus_bounds.js';
import {MathHandler} from './math_handler.js';
import {Output} from './output/output.js';
import {OutputCustomEvent} from './output/output_types.js';

type AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;
const Action = BridgeConstants.ChromeVoxRange.Action;
const TARGET = BridgeConstants.ChromeVoxRange.TARGET;

interface Point {
  x: number;
  y: number;
}

/**
 * An interface implemented by objects to observe ChromeVox range changes.
 * TODO(b/346347267): Convert to an interface post-TypeScript migration.
 */
export abstract class ChromeVoxRangeObserver {
  /** @param range The new range. */
  abstract onCurrentRangeChanged(
      range: CursorRange | null, fromEditing?: boolean): void;
}

/** Handles tracking of and changes to the ChromeVox range. */
export class ChromeVoxRange {
  private current_: CursorRange | null = null;
  private pageSel_: CursorRange | null = null;
  private previous_: CursorRange | null = null;
  private static observers_: ChromeVoxRangeObserver[] = [];

  static instance: ChromeVoxRange;

  private constructor() {}

  static init(): void {
    if (ChromeVoxRange.instance) {
      throw new Error('Cannot create more than one ChromeVoxRange');
    }
    ChromeVoxRange.instance = new ChromeVoxRange();

    BridgeHelper.registerHandler(
        TARGET, Action.CLEAR_CURRENT_RANGE, () => ChromeVoxRange.set(null));
  }

  static get current(): CursorRange | null {
    if (ChromeVoxRange.instance.current_?.isValid()) {
      return ChromeVoxRange.instance.current_;
    }
    return null;
  }

  static clearSelection(): void {
    ChromeVoxRange.instance.pageSel_ = null;
  }

  /** Return the current range, but focus recovery is not applied to it. */
  static getCurrentRangeWithoutRecovery(): CursorRange | null {
    return ChromeVoxRange.instance.current_;
  }

  /**
   * Check for loss of focus which results in us invalidating our current range.
   */
  static maybeResetFromFocus(): void {
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
      range: CursorRange, focus?: boolean, speechProps?: TtsSpeechProperties,
      skipSettingSelection?: boolean): void {
    ChromeVoxRange.instance.navigateTo_(
        range, focus, speechProps, skipSettingSelection);
  }

  /** Restores the last valid ChromeVox range. */
  static restoreLastValidRangeIfNeeded(): void {
    ChromeVoxRange.instance.restoreLastValidRangeIfNeeded_();
  }

  static set(newRange: CursorRange | null, fromEditing?: boolean): void {
    ChromeVoxRange.instance.set_(newRange, fromEditing);
  }

  /**
   * @return true if the selection is toggled on, false if it is toggled off.
   */
  static toggleSelection(): boolean {
    return ChromeVoxRange.instance.toggleSelection_();
  }

  // ================= Observer Functions =================

  static addObserver(observer: ChromeVoxRangeObserver): void {
    ChromeVoxRange.observers_.push(observer);
  }
  static removeObserver(observer: ChromeVoxRangeObserver): void {
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
   */
  private maybeResetFromFocus_(): void {
    chrome.automation.getFocus((focus: AutomationNode | undefined) => {
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
      // TODO(b/314203187): Not null asserted, check that this is correct.
      if (ChromeVoxState.instance!.talkBackEnabled &&
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
   * @param focus Focus the range; defaults to true.
   */
  private navigateTo_(
      range: CursorRange, focus?: boolean, speechProps?: TtsSpeechProperties,
      skipSettingSelection?: boolean): void {
    focus = focus ?? true;
    speechProps = speechProps ?? new TtsSpeechProperties();
    skipSettingSelection = skipSettingSelection ?? false;
    const prevRange = ChromeVoxRange.getCurrentRangeWithoutRecovery();

    // Specialization for math output.
    let skipOutput = false;
    if (MathHandler.init(range)) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      skipOutput = MathHandler.instance!.speak();
      focus = false;
    }

    if (focus) {
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
        // TODO(b/314203187): Not null asserted, check that this is correct.
        DesktopAutomationInterface.instance!.ignoreDocumentSelectionFromAction(
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
    } else if (!skipSettingSelection) {
      // Ensure we don't select the editable when we first encounter it.
      let lca: AutomationNode | null | undefined = null;
      if (range.start.node && prevRange?.start.node) {
        lca = AutomationUtil.getLeastCommonAncestor(
            prevRange!.start.node, range.start.node);
      }
      // TODO(b/314203187): Not null asserted, check that this is correct.
      if (!lca || lca.state![StateType.EDITABLE] ||
          !range.start.node.state![StateType.EDITABLE]) {
        range.select();
      }
    }

    o.withRichSpeechAndBraille(
         selectedRange ?? range, prevRange ?? undefined,
         OutputCustomEvent.NAVIGATE)
        .withInitialSpeechProperties(speechProps);

    if (msg) {
      o.format(msg);
    }

    if (!skipOutput) {
      o.go();
    }
  }

  private notifyObservers_(range: CursorRange | null, fromEditing?: boolean)
      : void {
    for (const observer of ChromeVoxRange.observers_) {
      observer.onCurrentRangeChanged(range, fromEditing);
    }
  }

  private restoreLastValidRangeIfNeeded_(): void {
    // Never restore range when TalkBack is enabled as commands such as
    // Search+Left, go directly to TalkBack.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (ChromeVoxState.instance!.talkBackEnabled) {
      return;
    }

    if (!this.current_?.isValid()) {
      this.current_ = this.previous_;
    }
  }

  private set_(newRange: CursorRange | null, fromEditing?: boolean): void {
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

    this.notifyObservers_(newRange, fromEditing);

    if (!this.current_) {
      FocusBounds.set([]);
      return;
    }

    const start = this.current_.start.node;
    start.makeVisible();

    chrome.metricsPrivate.recordBoolean(
        'Accessibility.ScreenReader.ScrollToImage',
        start.role === RoleType.IMAGE);

    start.setAccessibilityFocus();

    const root = AutomationUtil.getTopLevelRoot(start);
    if (!root || root.role === RoleType.DESKTOP || root === start) {
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    const loc = start.unclippedLocation!;
    const x = loc.left + loc.width / 2;
    const y = loc.top + loc.height / 2;
    const position: Point = {x, y};
    let url = root.docUrl!;
    url = url.substring(0, url.indexOf('#')) || url;
    ChromeVoxState.position[url] = position;
  }

  private setFocusToRange_(range: CursorRange, prevRange: CursorRange | null)
      : void {
    const start = range.start.node;
    const end = range.end.node;

    // First, see if we've crossed a root. Remove once webview handles focus
    // correctly.
    if (prevRange && prevRange.start.node && start) {
      const entered =
          AutomationUtil.getUniqueAncestors(prevRange.start.node, start);
      const isPluginOrIframe =
          AutomationPredicate.roles([RoleType.PLUGIN_OBJECT, RoleType.IFRAME]);

      entered.filter(isPluginOrIframe).forEach((container: AutomationNode) => {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        if (!container.state![StateType.FOCUSED]) {
          container.focus();
        }
      });
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (start.state![StateType.FOCUSED] || end.state![StateType.FOCUSED]) {
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    const isFocusableLinkOrControl = (node: AutomationNode): boolean =>
        node.state![StateType.FOCUSABLE] &&
        AutomationPredicate.linkOrControl(node);

    // Next, try to focus the start or end node.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!AutomationPredicate.structuralContainer(start) &&
        start.state![StateType.FOCUSABLE]) {
      if (!start.state![StateType.FOCUSED]) {
        start.focus();
      }
      return;
    } else if (
        !AutomationPredicate.structuralContainer(end) &&
        end.state![StateType.FOCUSABLE]) {
      if (!end.state![StateType.FOCUSED]) {
        end.focus();
      }
      return;
    }

    // If a common ancestor of |start| and |end| is a link, focus that.
    let ancestor = AutomationUtil.getLeastCommonAncestor(start, end);
    while (ancestor && ancestor.root === start.root) {
      if (isFocusableLinkOrControl(ancestor)) {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        if (!ancestor.state![StateType.FOCUSED]) {
          ancestor.focus();
        }
        return;
      }
      ancestor = ancestor.parent;
    }

    // If nothing is focusable, set the sequential focus navigation starting
    // point, which ensures that the next time you press Tab, you'll reach
    // the next or previous focusable node from |start|.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!start.state![StateType.OFFSCREEN]) {
      start.setSequentialFocusNavigationStartingPoint();
    }
  }

  /** @return true if the selection is toggled on, false if toggled off. */
  private toggleSelection_(): boolean {
    if (!this.pageSel_) {
      ChromeVox.earcons.playEarcon(EarconId.SELECTION);
      this.pageSel_ = ChromeVoxRange.current;
      // TODO(b/314203187): Not null asserted, check that this is correct.
      DesktopAutomationInterface.instance!.ignoreDocumentSelectionFromAction(
          true);
      return true;
    } else {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      const root = this.current_!.start.node.root;
      if (root && root.selectionStartObject && root.selectionEndObject &&
          !isNaN(Number(root.selectionStartOffset)) &&
          !isNaN(Number(root.selectionEndOffset))) {
        ChromeVox.earcons.playEarcon(EarconId.SELECTION_REVERSE);
        // TODO(b/314203187): Not null asserted, check that this is correct.
        const sel = new CursorRange(
            new Cursor(root.selectionStartObject, root.selectionStartOffset!),
            new Cursor(root.selectionEndObject, root.selectionEndOffset!));
        new Output()
            .format('@end_selection')
            .withSpeechAndBraille(sel, sel, OutputCustomEvent.NAVIGATE)
            .go();
        // TODO(b/314203187): Not null asserted, check that this is correct.
        DesktopAutomationInterface.instance!.ignoreDocumentSelectionFromAction(
            false);
      }
      this.pageSel_ = null;
      return false;
    }
  }
}

TestImportManager.exportForTesting(ChromeVoxRange, ChromeVoxRangeObserver);
