// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An EditableLine encapsulates all data concerning a line in the
 * automation tree necessary to provide output. Editable: an editable selection
 * (e.g. start/end offsets) get saved. Line: nodes/offsets at the beginning/end
 * of a line get saved.
 */

goog.provide('editing.EditableLine');

goog.scope(function() {
const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const Cursor = cursors.Cursor;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const FormType = LibLouis.FormType;
const Range = cursors.Range;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;
const Movement = cursors.Movement;
const Unit = cursors.Unit;

editing.EditableLine = class {
  /**
   * @param {!AutomationNode} startNode
   * @param {number} startIndex
   * @param {!AutomationNode} endNode
   * @param {number} endIndex
   * @param {boolean=} opt_baseLineOnStart  Controls whether to use
   *     |startNode| or |endNode| for Line computations. Selections are
   * automatically truncated up to either the line start or end.
   */
  constructor(startNode, startIndex, endNode, endIndex, opt_baseLineOnStart) {
    /** @private {!Cursor} */
    this.start_ = new Cursor(startNode, startIndex);
    this.start_ = this.start_.deepEquivalent || this.start_;
    /** @private {!Cursor} */
    this.end_ = new Cursor(endNode, endIndex);
    this.end_ = this.end_.deepEquivalent || this.end_;

    /** @private {AutomationNode|undefined} */
    this.endContainer_;

    // Update |startIndex| and |endIndex| if the calls above to
    // cursors.Cursor.deepEquivalent results in cursors to different container
    // nodes. The cursors can point directly to inline text boxes, in which case
    // we should not adjust the container start or end index.
    if (!AutomationPredicate.text(startNode) ||
        (this.start_.node !== startNode &&
         this.start_.node.parent !== startNode)) {
      startIndex =
          (this.start_.index === cursors.NODE_INDEX && this.start_.node.name) ?
          this.start_.node.name.length :
          this.start_.index;
    }

    if (!AutomationPredicate.text(endNode) ||
        (this.end_.node !== endNode && this.end_.node.parent !== endNode)) {
      endIndex =
          (this.end_.index === cursors.NODE_INDEX && this.end_.node.name) ?
          this.end_.node.name.length :
          this.end_.index;
    }

    /** @private {number} */
    this.localContainerStartOffset_ = startIndex;
    /** @private {number} */
    this.localContainerEndOffset_ = endIndex;

    // Computed members.
    /** @private {Spannable} */
    this.value_;
    /** @private {AutomationNode|undefined} */
    this.lineStart_;
    /** @private {AutomationNode|undefined} */
    this.lineEnd_;
    /** @private {AutomationNode|undefined} */
    this.startContainer_;
    /** @private {string} */
    this.startContainerValue_ = '';
    /** @private {AutomationNode|undefined} */
    this.lineStartContainer_;
    /** @private {number} */
    this.localLineStartContainerOffset_ = 0;
    /** @private {AutomationNode|undefined} */
    this.lineEndContainer_;
    /** @private {number} */
    this.localLineEndContainerOffset_ = 0;
    /** @type {RecoveryStrategy} */
    this.lineStartContainerRecovery_;

    this.computeLineData_(opt_baseLineOnStart);
  }

  /**
   * @param {boolean=} opt_baseLineOnStart Computes the line based on the start
   * node if true.
   * @private
   */
  computeLineData_(opt_baseLineOnStart) {
    // Note that we calculate the line based only upon |start_| or
    // |end_| even if they do not fall on the same line. It is up to
    // the caller to specify which end to base this line upon since it requires
    // reasoning about two lines.
    let nameLen = 0;
    const lineBase = opt_baseLineOnStart ? this.start_ : this.end_;
    const lineExtend = opt_baseLineOnStart ? this.end_ : this.start_;

    if (lineBase.node.name) {
      nameLen = lineBase.node.name.length;
    }

    this.value_ = new Spannable(lineBase.node.name || '', lineBase);
    if (lineBase.node === lineExtend.node) {
      this.value_.setSpan(lineExtend, 0, nameLen);
    }

    this.startContainer_ = this.start_.node;
    if (this.startContainer_.role === RoleType.INLINE_TEXT_BOX) {
      this.startContainer_ = this.startContainer_.parent;
    }
    this.startContainerValue_ =
        this.startContainer_.role === RoleType.TEXT_FIELD ?
        this.startContainer_.value || '' :
        this.startContainer_.name || '';
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
    const parents = [this.startContainer_];

    // Compute the start of line.
    let lineStart = this.lineStart_;

    // Hack: note underlying bugs require these hacks.
    while ((lineStart.previousOnLine && lineStart.previousOnLine.role) ||
           (lineStart.previousSibling && lineStart.previousSibling.lastChild &&
            lineStart.previousSibling.lastChild.nextOnLine === lineStart)) {
      if (lineStart.previousOnLine) {
        lineStart = lineStart.previousOnLine;
      } else {
        lineStart = lineStart.previousSibling.lastChild;
      }

      this.lineStart_ = lineStart;

      if (lineStart.role !== RoleType.INLINE_TEXT_BOX) {
        parents.unshift(lineStart);
      } else if (parents[0] !== lineStart.parent) {
        parents.unshift(lineStart.parent);
      }

      const prepend = new Spannable(lineStart.name, lineStart);
      prepend.append(this.value_);
      this.value_ = prepend;
    }
    this.lineStartContainer_ = this.lineStart_.parent;

    let lineEnd = this.lineEnd_;

    // Hack: note underlying bugs require these hacks.
    while ((lineEnd.nextOnLine && lineEnd.nextOnLine.role) ||
           (lineEnd.nextSibling &&
            lineEnd.nextSibling.previousOnLine === lineEnd)) {
      if (lineEnd.nextOnLine) {
        lineEnd = lineEnd.nextOnLine;
      } else {
        lineEnd = lineEnd.nextSibling.firstChild;
      }

      this.lineEnd_ = lineEnd;

      if (lineEnd.role !== RoleType.INLINE_TEXT_BOX) {
        parents.push(this.lineEnd_);
      } else if (parents[parents.length - 1] !== lineEnd.parent) {
        parents.push(this.lineEnd_.parent);
      }

      let annotation = lineEnd;
      if (lineEnd === this.end_.node) {
        annotation = this.end_;
      }

      this.value_.append(new Spannable(lineEnd.name, annotation));
    }
    this.lineEndContainer_ = this.lineEnd_.parent;

    // Finally, annotate with all parent static texts as NodeSpan's so that
    // braille routing can key properly into the node with an offset.
    // Note that both line start and end needs to account for
    // potential offsets into the static texts as follows.
    let textCountBeforeLineStart = 0, textCountAfterLineEnd = 0;
    let finder = this.lineStart_;
    while (finder.previousSibling) {
      finder = finder.previousSibling;
      textCountBeforeLineStart += finder.name ? finder.name.length : 0;
    }
    this.localLineStartContainerOffset_ = textCountBeforeLineStart;

    if (this.lineStartContainer_) {
      this.lineStartContainerRecovery_ =
          new TreePathRecoveryStrategy(this.lineStartContainer_);
    }

    finder = this.lineEnd_;
    while (finder.nextSibling) {
      finder = finder.nextSibling;
      textCountAfterLineEnd += finder.name ? finder.name.length : 0;
    }

    if (this.lineEndContainer_.name) {
      this.localLineEndContainerOffset_ =
          this.lineEndContainer_.name.length - textCountAfterLineEnd;
    }

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
        this.value_.setSpan(new Output.NodeSpan(parent, offset), prevLen, len);

        // Also, annotate this span if it is associated with line containre.
        if (parent === this.startContainer_) {
          this.value_.setSpan(parent, prevLen, len);
        }
      } catch (e) {
      }
    }
  }

  /**
   * Gets the selection offset based on the text content of this line.
   * @return {number}
   */
  get startOffset() {
    // It is possible that the start cursor points to content before this line
    // (e.g. in a multi-line selection).
    try {
      return this.value_.getSpanStart(this.start_) +
          (this.start_.index === cursors.NODE_INDEX ? 0 : this.start_.index);
    } catch (e) {
      // When that happens, fall back to the start of this line.
      return 0;
    }
  }

  /**
   * Gets the selection offset based on the text content of this line.
   * @return {number}
   */
  get endOffset() {
    try {
      return this.value_.getSpanStart(this.end_) +
          (this.end_.index === cursors.NODE_INDEX ? 0 : this.end_.index);
    } catch (e) {
      return this.value_.length;
    }
  }

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   * @return {number}
   */
  get localStartOffset() {
    return this.localContainerStartOffset_;
  }

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   * @return {number}
   */
  get localEndOffset() {
    return this.localContainerEndOffset_;
  }

  /**
   * Gets the start offset of the container, relative to the line text
   * content. The container refers to the static text parenting the inline
   * text box.
   * @return {number}
   */
  get containerStartOffset() {
    return this.value_.getSpanStart(this.startContainer_);
  }

  /**
   * Gets the end offset of the container, relative to the line text content.
   * The container refers to the static text parenting the inline text box.
   * @return {number}
   */
  get containerEndOffset() {
    return this.value_.getSpanEnd(this.startContainer_) - 1;
  }

  /**
   * The text content of this line.
   * @return {string} The text of this line.
   */
  get text() {
    return this.value_.toString();
  }

  /** @return {string} */
  get selectedText() {
    return this.value_.toString().substring(this.startOffset, this.endOffset);
  }

  /** @return {AutomationNode|undefined} */
  get startContainer() {
    return this.startContainer_;
  }

  /** @return {AutomationNode|undefined} */
  get endContainer() {
    return this.endContainer_;
  }

  /** @return {Spannable} */
  get value() {
    return this.value_;
  }

  /** @return {!cursors.Cursor} */
  get start() {
    return this.start_;
  }

  /** @return {!cursors.Cursor} */
  get end() {
    return this.end_;
  }

  /** @return {number} */
  get localContainerStartOffset() {
    return this.localContainerStartOffset_;
  }

  /** @return {number} */
  get localContainerEndOffset() {
    return this.localContainerEndOffset_;
  }

  /** @return {string} */
  get startContainerValue() {
    return this.startContainerValue_;
  }

  /** @return {!cursors.Cursor} */
  get startCursor() {
    return this.start_;
  }

  /** @return {boolean} */
  hasCollapsedSelection() {
    return this.start_.equals(this.end_);
  }

  /**
   * Returns whether this line has selection over text nodes.
   * @return {boolean}
   */
  hasTextSelection() {
    if (this.start_.node && this.end_.node) {
      return AutomationPredicate.text(this.start_.node) &&
          AutomationPredicate.text(this.end_.node);
    }

    return false;
  }

  /**
   * Returns true if |otherLine| surrounds the same line as |this|. Note that
   * the contents of the line might be different.
   * @param {editing.EditableLine} otherLine
   * @return {boolean}
   */
  isSameLine(otherLine) {
    // Equality is intentionally loose here as any of the state nodes can be
    // invalidated at any time. We rely upon the start/anchor of the line
    // staying the same.
    return (otherLine.lineStartContainer_ === this.lineStartContainer_ &&
            otherLine.localLineStartContainerOffset_ ===
                this.localLineStartContainerOffset_) ||
        (otherLine.lineEndContainer_ === this.lineEndContainer_ &&
         otherLine.localLineEndContainerOffset_ ===
             this.localLineEndContainerOffset_) ||
        (otherLine.lineStartContainerRecovery_.node ===
             this.lineStartContainerRecovery_.node &&
         otherLine.localLineStartContainerOffset_ ===
             this.localLineStartContainerOffset_);
  }

  /**
   * Returns true if |otherLine| surrounds the same line as |this| and has the
   * same selection.
   * @param {editing.EditableLine} otherLine
   * @return {boolean}
   */
  isSameLineAndSelection(otherLine) {
    return this.isSameLine(otherLine) &&
        this.startOffset === otherLine.startOffset &&
        this.endOffset === otherLine.endOffset;
  }

  /**
   * Returns whether this line comes before |otherLine| in document order.
   * @param {!editing.EditableLine} otherLine
   * @return {boolean}
   */
  isBeforeLine(otherLine) {
    if (this.isSameLine(otherLine) || !this.lineStartContainer_ ||
        !otherLine.lineStartContainer_) {
      return false;
    }
    return AutomationUtil.getDirection(
               this.lineStartContainer_, otherLine.lineStartContainer_) ===
        Dir.FORWARD;
  }

  /**
   * Performs a validation that this line still refers to a line given its
   * internally tracked state.
   * @return {boolean}
   */
  isValidLine() {
    if (!this.lineStartContainer_ || !this.lineEndContainer_) {
      return false;
    }

    const start = new cursors.Cursor(
        this.lineStartContainer_, this.localLineStartContainerOffset_);
    const end = new cursors.Cursor(
        this.lineEndContainer_, this.localLineEndContainerOffset_ - 1);
    const localStart = start.deepEquivalent || start;
    const localEnd = end.deepEquivalent || end;
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
      if (testStartNode.nextOnLine && testStartNode.nextOnLine.role) {
        testStartNode = testStartNode.nextOnLine;
      } else if (
          testStartNode.nextSibling &&
          testStartNode.nextSibling.previousOnLine === testStartNode) {
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
      if (testEndNode.previousOnLine && testEndNode.previousOnLine.role) {
        testEndNode = testEndNode.previousOnLine;
      } else if (
          testEndNode.previousSibling &&
          testEndNode.previousSibling.nextOnLine === testEndNode) {
        testEndNode = testEndNode.previousSibling;
      } else {
        break;
      }
    } while (testEndNode);

    return false;
  }

  /**
   * Speaks the line using text to speech.
   * @param {editing.EditableLine} prevLine
   */
  speakLine(prevLine) {
    let prev = (prevLine && prevLine.startContainer_.role) ?
        prevLine.startContainer_ :
        null;
    const lineNodes =
        /** @type {Array<!AutomationNode>} */ (this.value_.getSpansInstanceOf(
            /** @type {function()} */ (this.startContainer_.constructor)));
    let queueMode = QueueMode.CATEGORY_FLUSH;
    for (let i = 0, cur; cur = lineNodes[i]; i++) {
      if (cur.children.length) {
        continue;
      }

      const o = new Output()
                    .withRichSpeech(
                        Range.fromNode(cur),
                        prev ? Range.fromNode(prev) : Range.fromNode(cur),
                        Output.EventType.NAVIGATE)
                    .withQueueMode(queueMode);

      // Ignore whitespace only output except if it is leading content on the
      // line.
      if (!o.isOnlyWhitespace || i === 0) {
        o.go();
      }
      prev = cur;
      queueMode = QueueMode.QUEUE;
    }
  }
};
});  // goog.scope
