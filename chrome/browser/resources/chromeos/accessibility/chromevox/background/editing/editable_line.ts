// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An EditableLine encapsulates all data concerning a line in the
 * automation tree necessary to provide output. Editable: an editable selection
 * (e.g. start/end offsets) get saved. Line: nodes/offsets at the beginning/end
 * of a line get saved.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {constants} from '/common/constants.js';
import {Cursor, CURSOR_NODE_INDEX, CursorMovement, CursorUnit} from '/common/cursors/cursor.js';
import {CursorRange} from '/common/cursors/range.js';
import {RecoveryStrategy, TreePathRecoveryStrategy} from '/common/cursors/recovery_strategy.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Spannable} from '../../common/spannable.js';
import {Output} from '../output/output.js';
import {OutputCustomEvent, OutputNodeSpan} from '../output/output_types.js';

type AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;
const Movement = CursorMovement;
const Unit = CursorUnit;

interface StartMetadata {
  lineStart?: AutomationNode;
  value: Spannable;
  textCountBeforeLineStart: number;
}
interface EndMetadata {
  lineEnd?: AutomationNode;
  value: Spannable;
  textCountAfterLineEnd: number;
}

export class EditableLine {
  private start_: Cursor;
  private end_: Cursor;
  private localContainerStartOffset_: number;
  private localContainerEndOffset_: number;

  // Computed members.
  private endContainer_?: AutomationNode;
  private lineStart_?: AutomationNode;
  private lineStartContainer_?: AutomationNode;
  private lineStartContainerRecovery_?: RecoveryStrategy;
  private lineEnd_?: AutomationNode;
  private lineEndContainer_?: AutomationNode;
  private localLineStartContainerOffset_ = 0;
  private localLineEndContainerOffset_ = 0;
  private startContainer_?: AutomationNode;
  private startContainerValue_ = '';
  private value_: Spannable;

  /**
   * Controls whether line computations include offscreen inline text boxes.
   * Note that a caller should have this set prior to creating a line.
   */
  static includeOffscreen = true;

  /**
   * @param baseLineOnStart  Controls whether to use |startNode| or |endNode|
   *     for Line computations. Selections are automatically truncated up to
   *     either the line start or end.
   */
  constructor(
      startNode: AutomationNode, startIndex: number, endNode: AutomationNode,
      endIndex: number, baseLineOnStart?: boolean) {
    this.start_ = new Cursor(startNode, startIndex);
    this.start_ = this.start_.deepEquivalent ?? this.start_;

    this.end_ = new Cursor(endNode, endIndex);
    this.end_ = this.end_.deepEquivalent ?? this.end_;

    // Update |startIndex| and |endIndex| if the calls above to
    // Cursor.deepEquivalent results in cursors to different container
    // nodes. The cursors can point directly to inline text boxes, in which case
    // we should not adjust the container start or end index.
    if (!AutomationPredicate.text(startNode) ||
        (this.start_.node !== startNode &&
         this.start_.node.parent !== startNode)) {
      startIndex =
          (this.start_.index === CURSOR_NODE_INDEX && this.start_.node.name) ?
          this.start_.node.name.length :
          this.start_.index;
    }

    if (!AutomationPredicate.text(endNode) ||
        (this.end_.node !== endNode && this.end_.node.parent !== endNode)) {
      endIndex =
          (this.end_.index === CURSOR_NODE_INDEX && this.end_.node.name) ?
          this.end_.node.name.length :
          this.end_.index;
    }

    this.localContainerStartOffset_ = startIndex;
    this.localContainerEndOffset_ = endIndex;

    // Note that we calculate the line based only upon |start_| or
    // |end_| even if they do not fall on the same line. It is up to
    // the caller to specify which end to base this line upon since it requires
    // reasoning about two lines.
    let nameLen = 0;
    const lineBase = baseLineOnStart ? this.start_ : this.end_;
    const lineExtend = baseLineOnStart ? this.end_ : this.start_;

    if (lineBase.node.name) {
      nameLen = lineBase.node.name.length;
    }

    this.value_ = new Spannable(lineBase.node.name ?? '', lineBase);
    if (lineBase.node === lineExtend.node) {
      this.value_.setSpan(lineExtend, 0, nameLen);
    }

    this.startContainer_ = this.start_.node;
    if (this.startContainer_.role === RoleType.INLINE_TEXT_BOX) {
      this.startContainer_ = this.startContainer_.parent;
    }
    this.startContainerValue_ =
        this.startContainer_?.role === RoleType.TEXT_FIELD ?
        this.startContainer_?.value ?? '' :
        this.startContainer_?.name ?? '';
    this.endContainer_ = this.end_.node;
    if (this.endContainer_.role === RoleType.INLINE_TEXT_BOX) {
      this.endContainer_ = this.endContainer_.parent;
    }

    // Initialize defaults.
    this.lineStart_ = lineBase.node;
    this.lineEnd_ = this.lineStart_;
    this.lineStartContainer_ = this.lineStart_.parent;
    this.lineEndContainer_ = this.lineStart_.parent;

    // Annotate each chunk with its associated inline text box node.
    this.value_.setSpan(this.lineStart_, 0, nameLen);

    // Also, track the nodes necessary for selection (either their parents, in
    // the case of inline text boxes, or the node itself).
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const parents: AutomationNode[] = [this.startContainer_!];

    // Keep track of visited nodes to ensure we don't visit the same node twice.
    // Workaround for crbug.com/1203840.
    const visited = new WeakSet();
    if (this.lineStart_) {
      visited.add(this.lineStart_);
    }

    const startData = this.computeLineStartMetadata_(
        this.lineStart_, this.value_, parents, visited);
    this.lineStart_ = startData.lineStart;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.lineStartContainer_ = this.lineStart_!.parent;
    this.value_ = startData.value;
    const textCountBeforeLineStart = startData.textCountBeforeLineStart;
    this.localLineStartContainerOffset_ = textCountBeforeLineStart;
    if (this.lineStartContainer_) {
      this.lineStartContainerRecovery_ =
          new TreePathRecoveryStrategy(this.lineStartContainer_);
    }

    const endData = this.computeLineEndMetadata_(
        this.lineEnd_, this.value_, parents, visited);
    this.lineEnd_ = endData.lineEnd;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.lineEndContainer_ = this.lineEnd_!.parent;
    this.value_ = endData.value;
    const textCountAfterLineEnd = endData.textCountAfterLineEnd;
    if (this.lineEndContainer_!.name) {
      this.localLineEndContainerOffset_ =
          this.lineEndContainer_!.name.length - textCountAfterLineEnd;
    }

    // Annotate with all parent static texts as NodeSpans so that braille
    // routing can key properly into the node with an offset.
    this.value_ = this.annotateWithParents_(
        this.value_, parents, textCountBeforeLineStart, textCountAfterLineEnd);
  }

  private computeLineStartMetadata_(
      scanNode: AutomationNode | undefined, value: Spannable,
      parents: AutomationNode[], visited: WeakSet<AutomationNode>)
      : StartMetadata {
    let lineStart = scanNode;
    if (scanNode) {
      scanNode = this.getPreviousOnLine_(scanNode);
    }
    // Compute |lineStart|.
    while (scanNode && !visited.has(scanNode)) {
      visited.add(scanNode);
      lineStart = scanNode;

      if (scanNode.role !== RoleType.INLINE_TEXT_BOX) {
        parents.unshift(scanNode);
      } else if (parents[0] !== scanNode.parent) {
        parents.unshift(scanNode.parent!);
      }

      const prepend = new Spannable(scanNode.name, scanNode);
      prepend.append(value);
      value = prepend;

      scanNode = this.getPreviousOnLine_(scanNode);
    }

    // Note that we need to account for potential offsets into the static texts
    // as follows.
    let textCountBeforeLineStart = 0;
    let finder = lineStart;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    while (finder!.previousSibling &&
           (EditableLine.includeOffscreen ||
            !finder!.previousSibling.state![StateType.OFFSCREEN])) {
      finder = finder!.previousSibling;
      textCountBeforeLineStart += finder.name?.length ?? 0;
    }

    return {lineStart, value, textCountBeforeLineStart};
  }

  private computeLineEndMetadata_(
      scanNode: AutomationNode | undefined, value: Spannable,
      parents: AutomationNode[], visited: WeakSet<AutomationNode>)
      : EndMetadata {
    let lineEnd = scanNode;
    if (scanNode) {
      scanNode = this.getNextOnLine_(scanNode);
    }
    // Compute |lineEnd|.
    while (scanNode && !visited.has(scanNode)) {
      visited.add(scanNode);
      lineEnd = scanNode;

      if (scanNode.role !== RoleType.INLINE_TEXT_BOX) {
        parents.push(scanNode);
      } else if (parents[parents.length - 1] !== scanNode.parent) {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        parents.push(scanNode.parent!);
      }

      let annotation: AutomationNode | Cursor = scanNode;
      if (scanNode === this.end_.node) {
        annotation = this.end_;
      }

      value.append(new Spannable(scanNode.name, annotation));

      scanNode = this.getNextOnLine_(scanNode);
    }

    // Note that we need to account for potential offsets into the static texts
    // as follows.
    let textCountAfterLineEnd = 0;
    let finder: AutomationNode | undefined = lineEnd;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    while (finder!.nextSibling &&
           (EditableLine.includeOffscreen ||
            !finder!.nextSibling.state![StateType.OFFSCREEN])) {
      finder = finder!.nextSibling;
      textCountAfterLineEnd += finder.name?.length ?? 0;
    }

    return {lineEnd, value, textCountAfterLineEnd};
  }

  private annotateWithParents_(
      value: Spannable, parents: AutomationNode[],
      textCountBeforeLineStart: number, textCountAfterLineEnd: number)
      : Spannable {
    let len = 0;
    for (let i = 0; i < parents.length; i++) {
      const parent = parents[i];

      if (!parent.name) {
        continue;
      }

      const prevLen = len;
      let currentLen = parent.name.length;
      let offset = 0;

      // Subtract off the text count before when at the start of line.
      if (i === 0) {
        currentLen -= textCountBeforeLineStart;
        offset = textCountBeforeLineStart;
      }

      // Subtract text count after when at the end of the line.
      if (i === parents.length - 1) {
        currentLen -= textCountAfterLineEnd;
      }

      len += currentLen;

      try {
        value.setSpan(new OutputNodeSpan(parent, offset), prevLen, len);

        // Also, annotate this span if it is associated with line container.
        if (parent === this.startContainer_) {
          value.setSpan(parent, prevLen, len);
        }
      } catch (e) {
        console.error(e);
      }
    }
    return value;
  }

  private getNextOnLine_(node: AutomationNode): AutomationNode | undefined {
    const nextOnLine = node.nextOnLine;
    const nextSibling = node.nextSibling;
    if (nextOnLine?.role) {
      // Ensure that there is a next-on-line node. The role can be undefined
      // for an object that has been destroyed since the object was first
      // cached.
      return nextOnLine;
    }

    if (nextSibling?.previousOnLine === node) {
      // Catches potential breaks in the chain of next-on-line nodes.
      return nextSibling.firstChild;
    }

    return undefined;
  }

  private getPreviousOnLine_(node: AutomationNode): AutomationNode | undefined {
    const previousLine = node.previousOnLine;
    const previousSibling = node.previousSibling;
    if (previousLine?.role) {
      // Ensure that there is a previous-on-line node. The role can be undefined
      // for an object that has been destroyed since the object was first
      // cached.
      return previousLine;
    }

    if (previousSibling?.lastChild?.nextOnLine === node) {
      // Catches potential breaks in the chain of previous-on-line nodes.
      return previousSibling.lastChild;
    }

    return undefined;
  }

  /** Gets the selection offset based on the text content of this line. */
  get startOffset(): number {
    // It is possible that the start cursor points to content before this line
    // (e.g. in a multi-line selection).
    try {
      return this.value_.getSpanStart(this.start_) +
          (this.start_.index === CURSOR_NODE_INDEX ? 0 : this.start_.index);
    } catch (e) {
      // When that happens, fall back to the start of this line.
      return 0;
    }
  }

  /** Gets the selection offset based on the text content of this line. */
  get endOffset(): number {
    try {
      return this.value_.getSpanStart(this.end_) +
          (this.end_.index === CURSOR_NODE_INDEX ? 0 : this.end_.index);
    } catch (e) {
      // When that happens, fall back to the end of this line.
      return this.value_.length;
    }
  }

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   */
  get localStartOffset(): number {
    return this.localContainerStartOffset_;
  }

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   */
  get localEndOffset(): number {
    return this.localContainerEndOffset_;
  }

  /**
   * Gets the start offset of the container, relative to the line text
   * content. The container refers to the static text parenting the inline
   * text box.
   */
  get containerStartOffset(): number {
    return this.value_.getSpanStart(this.startContainer_);
  }

  /**
   * Gets the end offset of the container, relative to the line text content.
   * The container refers to the static text parenting the inline text box.
   */
  get containerEndOffset(): number {
    return this.value_.getSpanEnd(this.startContainer_) - 1;
  }

  /** @return The text content of this line. */
  get text(): string {
    return this.value_.toString();
  }

  get selectedText(): string {
    return this.value_.toString().substring(this.startOffset, this.endOffset);
  }

  get startContainer(): AutomationNode | undefined {
    return this.startContainer_;
  }

  get endContainer(): AutomationNode | undefined {
    return this.endContainer_;
  }

  get value(): Spannable {
    return this.value_;
  }

  get start(): Cursor {
    return this.start_;
  }

  get end(): Cursor {
    return this.end_;
  }

  get localContainerStartOffset(): number {
    return this.localContainerStartOffset_;
  }

  get localContainerEndOffset(): number {
    return this.localContainerEndOffset_;
  }

  get startContainerValue(): string {
    return this.startContainerValue_;
  }

  hasCollapsedSelection(): boolean {
    return this.start_.equals(this.end_);
  }

  /** @return Whether this line has selection over text nodes. */
  hasTextSelection(): boolean {
    if (this.start_.node && this.end_.node) {
      return AutomationPredicate.text(this.start_.node) &&
          AutomationPredicate.text(this.end_.node);
    }

    return false;
  }

  /**
   * Returns true if |otherLine| surrounds the same line as |this|. Note that
   * the contents of the line might be different.
   */
  isSameLine(otherLine: EditableLine): boolean {
    // Equality is intentionally loose here as any of the state nodes can be
    // invalidated at any time. We rely upon the start/anchor of the line
    // staying the same.
    const startNodeAndOffsetMatch =
        otherLine.lineStartContainer_ === this.lineStartContainer_ &&
        otherLine.localLineStartContainerOffset_ ===
            this.localLineStartContainerOffset_;
    const endNodeAndOffsetMatch =
        otherLine.lineEndContainer_ === this.lineEndContainer_ &&
        otherLine.localLineEndContainerOffset_ ===
            this.localLineEndContainerOffset_;
    const recoveryNodeAndOffsetMatch =
        otherLine.lineStartContainerRecovery_?.node ===
            this.lineStartContainerRecovery_?.node &&
        otherLine.localLineStartContainerOffset_ ===
            this.localLineStartContainerOffset_;


    return startNodeAndOffsetMatch || endNodeAndOffsetMatch ||
        recoveryNodeAndOffsetMatch;
  }

  /**
   * Returns true if |otherLine| surrounds the same line as |this| and has the
   * same selection.
   */
  isSameLineAndSelection(otherLine: EditableLine): boolean {
    return this.isSameLine(otherLine) &&
        this.startOffset === otherLine.startOffset &&
        this.endOffset === otherLine.endOffset;
  }

  /** Returns whether this line comes before |otherLine| in document order. */
  isBeforeLine(otherLine: EditableLine): boolean {
    if (!this.lineStartContainer_ || !otherLine.lineStartContainer_) {
      return false;
    }

    if (this.isSameLine(otherLine)) {
      return this.endOffset <= otherLine.endOffset;
    }

    return AutomationUtil.getDirection(
               this.lineStartContainer_, otherLine.lineStartContainer_) ===
        Dir.FORWARD;
  }

  /**
   * Performs a validation that this line still refers to a line given its
   * internally tracked state.
   */
  isValidLine(): boolean {
    if (!this.lineStartContainer_ || !this.lineEndContainer_) {
      return false;
    }

    const start = new Cursor(
        this.lineStartContainer_, this.localLineStartContainerOffset_);
    const end = new Cursor(
        this.lineEndContainer_, this.localLineEndContainerOffset_ - 1);
    const localStart = start.deepEquivalent ?? start;
    const localEnd = end.deepEquivalent ?? end;
    const localStartNode = localStart.node;
    const localEndNode = localEnd.node;

    // Unfortunately, there are asymmetric errors in lines, so we need to
    // check in both directions.
    let testStartNode = localStartNode;
    do {
      if (testStartNode === localEndNode) {
        return true;
      }

      // Hack/workaround for broken *OnLine links.
      if (testStartNode.nextOnLine?.role) {
        testStartNode = testStartNode.nextOnLine;
      } else if (testStartNode.nextSibling?.previousOnLine === testStartNode) {
        testStartNode = testStartNode.nextSibling;
      } else {
        break;
      }
    } while (testStartNode);

    let testEndNode = localEndNode;
    do {
      if (testEndNode === localStartNode) {
        return true;
      }

      // Hack/workaround for broken *OnLine links.
      if (testEndNode.previousOnLine?.role) {
        testEndNode = testEndNode.previousOnLine;
      } else if (testEndNode.previousSibling?.nextOnLine === testEndNode) {
        testEndNode = testEndNode.previousSibling;
      } else {
        break;
      }
    } while (testEndNode);

    return false;
  }

  /** Speaks the line using text to speech. */
  speakLine(prevLine: EditableLine): void {
    // Detect when the entire line is just a breaking space. This occurs on
    // Google Docs and requires that we speak it as a new line. However, we
    // still need to account for all of the possible rich output occurring from
    // ancestors of line nodes.
    const isLineBreakingSpace = this.text === '\u00a0';

    // TODO(b/314203187): Not null asserted, check that this is correct.
    const prev =
        prevLine?.startContainer_!.role ? prevLine.startContainer_ : null;
    const lineNodes: AutomationNode[] =
        this.value_.getSpansInstanceOf(this.startContainer_!.constructor);
    const speakNodeAtIndex = (index: number, prev: AutomationNode): void => {
      const cur = lineNodes[index];
      if (!cur) {
        return;
      }

      if (cur.children.length) {
        speakNodeAtIndex(++index, cur);
        return;
      }

      const o = new Output();

      if (isLineBreakingSpace) {
        // Apply a replacement for \u00a0 to \n.
        o.withSpeechTextReplacement('\u00a0', '\n');
      }

      o.withRichSpeech(
           CursorRange.fromNode(cur),
           prev ? CursorRange.fromNode(prev) : CursorRange.fromNode(cur),
           OutputCustomEvent.NAVIGATE)
          .onSpeechEnd(() => speakNodeAtIndex(++index, cur));

      // Ignore whitespace only output except if it is leading content on the
      // line.
      if (!o.isOnlyWhitespace || index === 0) {
        o.go();
      } else {
        speakNodeAtIndex(++index, cur);
      }
    };

    // TODO(b/314203187): Not null asserted, check that this is correct.
    speakNodeAtIndex(0, prev!);
  }

  /**
   * Creates a range around the character to the right of the line's starting
   * position.
   */
  createCharRange(): CursorRange {
    const start = this.start_;
    let end = start.move(Unit.CHARACTER, Movement.DIRECTIONAL, Dir.FORWARD);

    // The following conditions detect when|start|moves across a node boundary
    // to|end|.
    if (start.node !== end.node ||
        // When |start| and |end| are equal, that means we've reached
        // the end of the document. This is a node boundary as well.
        start.equals(end)) {
      end = new Cursor(start.node, start.index + 1);
    }
    return new CursorRange(start, end);
  }

  createWordRange(shouldMoveToPreviousWord: boolean): CursorRange {
    const pos = this.start_;
    // When movement goes to the end of a word, we actually want to
    // describe the word itself; this is considered the previous word so
    // impacts the movement type below. We can give further context e.g.
    // by saying "end of word", if we chose to be more verbose.
    const start = pos.move(
        Unit.WORD,
        shouldMoveToPreviousWord ? Movement.DIRECTIONAL : Movement.BOUND,
        Dir.BACKWARD);
    const end = start.move(Unit.WORD, Movement.BOUND, Dir.FORWARD);
    return new CursorRange(start, end);
  }
}

TestImportManager.exportForTesting(EditableLine);
