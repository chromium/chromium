// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Processes events related to editing text and emits the
 * appropriate spoken and braille feedback.
 */

goog.provide('editing.TextEditHandler');

goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('IntentHandler');
goog.require('Output');
goog.require('Output.EventType');
goog.require('TreePathRecoveryStrategy');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('editing.EditableLine');
goog.require('BrailleBackground');
goog.require('ChromeVoxEditableTextBase');
goog.require('LibLouis.FormType');

goog.scope(function() {
const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationIntent = chrome.automation.AutomationIntent;
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

/**
 * A handler for automation events in a focused text field or editable root
 * such as a |contenteditable| subtree.
 */
editing.TextEditHandler = class {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    /** @const {!AutomationNode} @private */
    this.node_ = node;

    if (!node.state[StateType.EDITABLE]) {
      throw '|node| must be editable.';
    }

    /** @private {AutomationEditableText} */
    this.editableText_;

    chrome.automation.getDesktop(function(desktop) {
      // ChromeVox handles two general groups of text fields:
      // A rich text field is one where selection gets placed on a DOM
      // descendant to a root text field. This is one of:
      // - content editables (detected via richly editable state)
      // - text areas (<textarea>) detected via its html tag
      //
      // A non-rich text field is one where accessibility only provides a value,
      // and a pair of numbers for the selection start and end. ChromeVox places
      // single-lined text fields, including those from web content, and ARC++
      // in this group. In addition, multiline ARC++ text fields are treated
      // this way.
      const useRichText =
          node.state[StateType.RICHLY_EDITABLE] || node.htmlTag === 'textarea';

      this.editableText_ = useRichText ? new AutomationRichEditableText(node) :
                                         new AutomationEditableText(node);
    }.bind(this));
  }

  /** @return {!AutomationNode} */
  get node() {
    return this.node_;
  }

  /**
   * Receives the following kinds of events when the node provided to the
   * constructor is focused: |focus|, |textChanged|, |textSelectionChanged| and
   * |valueInTextFieldChanged|.
   * An implementation of this method should emit the appropriate braille and
   * spoken feedback for the event.
   * @param {!ChromeVoxEvent} evt
   */
  onEvent(evt) {
    if (!evt.target.state.focused || evt.target !== this.node_) {
      return;
    }

    this.editableText_.onUpdate(evt.eventFrom, evt.intents);
  }

  /**
   * Returns true if selection starts at the first line.
   * @return {boolean}
   */
  isSelectionOnFirstLine() {
    return this.editableText_.isSelectionOnFirstLine();
  }

  /**
   * Returns true if selection ends at the last line.
   * @return {boolean}
   */
  isSelectionOnLastLine() {
    return this.editableText_.isSelectionOnLastLine();
  }

  /**
   * Moves range to after this text field.
   */
  moveToAfterEditText() {
    const after = AutomationUtil.findNextNode(
                      this.node_, Dir.FORWARD, AutomationPredicate.object,
                      {skipInitialSubtree: true}) ||
        this.node_;
    ChromeVoxState.instance.navigateToRange(
        cursors.Range.fromNode(after), true, {}, true);
  }

  /**
   * @param {!AutomationNode} node The root editable node, i.e. the root of a
   *     contenteditable subtree or a text field.
   * @return {editing.TextEditHandler}
   */
  static createForNode(node) {
    if (!node.state.editable) {
      throw new Error('Expected editable node.');
    }

    return new editing.TextEditHandler(node);
  }
};


/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields.
 */
const AutomationEditableText = class extends ChromeVoxEditableTextBase {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    if (!node.state.editable) {
      throw Error('Node must have editable state set to true.');
    }
    const value = AutomationEditableText.getProcessedValue_(node) || '';
    const lineBreaks = AutomationEditableText.getLineBreaks_(value);
    const start = node.textSelStart;
    const end = node.textSelEnd;

    super(
        value, Math.min(start, end, value.length),
        Math.min(Math.max(start, end), value.length),
        node.state[StateType.PROTECTED] /**password*/, ChromeVox.tts);
    /** @private {!Array<number>} */
    this.lineBreaks_ = lineBreaks;
    /** @override */
    this.multiline = node.state[StateType.MULTILINE] || false;
    /** @type {!AutomationNode} @private */
    this.node_ = node;
  }

  /**
   * Called when the text field has been updated.
   * @param {string|undefined} eventFrom
   * @param {!Array<AutomationIntent>} intents
   */
  onUpdate(eventFrom, intents) {
    const oldValue = this.value;
    const oldStart = this.start;
    const oldEnd = this.end;
    const newValue =
        AutomationEditableText.getProcessedValue_(this.node_) || '';
    if (oldValue !== newValue) {
      this.lineBreaks_ = AutomationEditableText.getLineBreaks_(newValue);
    }

    const textChangeEvent = new TextChangeEvent(
        newValue, Math.min(this.node_.textSelStart || 0, newValue.length),
        Math.min(this.node_.textSelEnd || 0, newValue.length),
        true /* triggered by user */);
    this.changed(textChangeEvent);
    this.outputBraille_(oldValue, oldStart, oldEnd);
  }

  /**
   * Returns true if selection starts on the first line.
   */
  isSelectionOnFirstLine() {
    return this.getLineIndex(this.start) === 0;
  }

  /**
   * Returns true if selection ends on the last line.
   */
  isSelectionOnLastLine() {
    return this.getLineIndex(this.end) >= this.lineBreaks_.length - 1;
  }

  /** @override */
  getLineIndex(charIndex) {
    let lineIndex = 0;
    while (charIndex > this.lineBreaks_[lineIndex]) {
      lineIndex++;
    }
    return lineIndex;
  }

  /** @override */
  getLineStart(lineIndex) {
    if (lineIndex === 0) {
      return 0;
    }

    // The start of this line is defined as the line break of the previous line
    // + 1 (the hard line break).
    return this.lineBreaks_[lineIndex - 1] + 1;
  }

  /** @override */
  getLineEnd(lineIndex) {
    return this.lineBreaks_[lineIndex];
  }

  /** @private */
  outputBraille_(oldValue, oldStart, oldEnd) {
    let lineIndex = this.getLineIndex(this.start);
    // Output braille at the end of the selection that changed, if start and end
    // differ.
    if (this.start !== this.end && this.start === oldStart) {
      lineIndex = this.getLineIndex(this.end);
    }
    const lineStart = this.getLineStart(lineIndex);
    let lineText =
        this.value.substr(lineStart, this.getLineEnd(lineIndex) - lineStart);

    if (lineIndex === 0) {
      lineText += ' ' +
          Msgs.getMsg(this.multiline ? 'tag_textarea_brl' : 'role_textbox_brl');
    }
    const startIndex = this.start - lineStart;
    const endIndex = this.end - lineStart;

    // If the line is not the last line, and is empty, insert an explicit line
    // break so that braille output is correctly cleared and has a position for
    // a caret to be shown.
    if (lineText === '' && lineIndex < this.lineBreaks_.length - 1) {
      lineText = '\n';
    }

    const spannable = new Spannable(lineText, new Output.NodeSpan(this.node_));
    ChromeVox.braille.write(
        new NavBraille({text: spannable, startIndex, endIndex}));
  }

  /**
   * @param {!AutomationNode} node
   * @return {string|undefined}
   * @private
   */
  static getProcessedValue_(node) {
    const value = node.value;
    return (value && node.inputType === 'tel') ? value['trimEnd']() : value;
  }

  /**
   * @param {string} value
   * @return {!Array<number>}
   * @private
   */
  static getLineBreaks_(value) {
    const lineBreaks = [];
    const lines = value.split('\n');
    for (let i = 0, total = 0; i < lines.length; i++) {
      total += lines[i].length;
      lineBreaks[i] = total;

      // Account for the line break itself.
      total++;
    }
    return lineBreaks;
  }
};


/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields using anchor and focus selection.
 */
const AutomationRichEditableText = class extends AutomationEditableText {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    super(node);

    const root = this.node_.root;
    if (!root || !root.selectionStartObject || !root.selectionEndObject ||
        root.selectionStartOffset === undefined ||
        root.selectionEndOffset === undefined) {
      return;
    }

    this.startLine_ = new editing.EditableLine(
        root.selectionStartObject, root.selectionStartOffset,
        root.selectionStartObject, root.selectionStartOffset);
    this.endLine_ = new editing.EditableLine(
        root.selectionEndObject, root.selectionEndOffset,
        root.selectionEndObject, root.selectionEndOffset);

    this.line_ = new editing.EditableLine(
        root.selectionStartObject, root.selectionStartOffset,
        root.selectionEndObject, root.selectionEndOffset);

    this.updateIntraLineState_(this.line_);

    /** @private {boolean} */
    this.misspelled = false;
    /** @private {boolean} */
    this.grammarError = false;

    /** @private {number|undefined} */
    this.fontSize_;
    /** @private {string|undefined} */
    this.fontColor_;
    /** @private {boolean|undefined} */
    this.linked_;
    /** @private {boolean|undefined} */
    this.subscript_;
    /** @private {boolean|undefined} */
    this.superscript_;
    /** @private {boolean} */
    this.bold_ = false;
    /** @private {boolean} */
    this.italic_ = false;
    /** @private {boolean} */
    this.underline_ = false;
    /** @private {boolean} */
    this.lineThrough_ = false;
    /** @private {string|undefined} */
    this.fontFamily_;
  }

  /** @override */
  isSelectionOnFirstLine() {
    let deep = this.line_.end.node;
    while (deep.previousOnLine) {
      deep = deep.previousOnLine;
    }
    const next = AutomationUtil.findNextNode(
        deep, Dir.BACKWARD, AutomationPredicate.inlineTextBox);
    if (!next) {
      return true;
    }
    const exited = AutomationUtil.getUniqueAncestors(next, deep);
    return !!exited.find(function(item) {
      return item === this.node_;
    }.bind(this));
  }

  /** @override */
  isSelectionOnLastLine() {
    let deep = this.line_.end.node;
    while (deep.nextOnLine) {
      deep = deep.nextOnLine;
    }
    const next = AutomationUtil.findNextNode(
        deep, Dir.FORWARD, AutomationPredicate.inlineTextBox);
    if (!next) {
      return true;
    }
    const exited = AutomationUtil.getUniqueAncestors(next, deep);
    return !!exited.find(function(item) {
      return item === this.node_;
    }.bind(this));
  }

  /** @override */
  onUpdate(eventFrom, intents) {
    const root = this.node_.root;
    if (!root.selectionStartObject || !root.selectionEndObject ||
        root.selectionStartOffset === undefined ||
        root.selectionEndOffset === undefined) {
      return;
    }

    const startLine = new editing.EditableLine(
        root.selectionStartObject, root.selectionStartOffset,
        root.selectionStartObject, root.selectionStartOffset);
    const endLine = new editing.EditableLine(
        root.selectionEndObject, root.selectionEndOffset,
        root.selectionEndObject, root.selectionEndOffset);

    const prevStartLine = this.startLine_;
    const prevEndLine = this.endLine_;
    this.startLine_ = startLine;
    this.endLine_ = endLine;

    const baseLineOnStart = prevEndLine.isSameLineAndSelection(endLine);
    const isSameSelection =
        baseLineOnStart && prevStartLine.isSameLineAndSelection(startLine);

    let cur;
    if (isSameSelection && this.line_) {
      // Nothing changed, return.
      return;
    } else {
      cur = new editing.EditableLine(
          root.selectionStartObject, root.selectionStartOffset,
          root.selectionEndObject, root.selectionEndOffset, baseLineOnStart);
    }
    const prev = this.line_;
    this.line_ = cur;

    this.handleSpeech_(
        cur, prev, startLine, endLine, prevStartLine, prevEndLine,
        baseLineOnStart, intents);
    this.handleBraille_();
  }

  /**
   * @param {!editing.EditableLine} cur
   * @param {!editing.EditableLine} prev
   * @param {!editing.EditableLine} startLine
   * @param {!editing.EditableLine} endLine
   * @param {!editing.EditableLine} prevStartLine
   * @param {!editing.EditableLine} prevEndLine
   * @param {boolean} baseLineOnStart
   * @param {!Array<AutomationIntent>} intents
   * @private
   */
  handleSpeech_(
      cur, prev, startLine, endLine, prevStartLine, prevEndLine,
      baseLineOnStart, intents) {
    // During continuous read, skip speech (which gets handled in
    // CommandHandler). We use the speech end callback to trigger additional
    // speech.
    // Also, skip speech based on the predicate.
    if (ChromeVoxState.isReadingContinuously ||
        AutomationPredicate.shouldOnlyOutputSelectionChangeInBraille(
            this.node_)) {
      this.updateIntraLineState_(cur);
      return;
    }

    // Before entering into our state machine below, use selected intents to
    // decipher ambiguous cases.
    if (this.maybeSpeakUsingIntents_(intents, cur, prev)) {
      return;
    }

    // Selection stayed within the same line(s) and didn't cross into new lines.

    // We must validate the previous lines as state changes in the accessibility
    // tree may have invalidated the lines.
    if (startLine.isSameLine(prevStartLine) &&
        endLine.isSameLine(prevEndLine)) {
      // Intra-line changes.

      if (cur.hasTextSelection()) {
        if (!prev.hasTextSelection() && cur.hasCollapsedSelection() &&
            cur.startOffset > prev.startOffset) {
          // EditableTextBase cannot handle this state transition (moving
          // forward from rich text to a caret in plain text). Fall back to
          // simply reading the character to the right of the caret. We achieve
          // this by updating the indices first, then sending the new change.

          // These members come from EditableTextBase.
          this.start = cur.endOffset > 0 ? cur.endOffset - 1 : 0;
          this.end = this.start;
        }
        // Delegate to EditableTextBase (via |changed|), which handles plain
        // text state output.
        let text = cur.text;
        if (text === '\n') {
          text = '';
        }
        this.changed(
            new TextChangeEvent(text, cur.startOffset, cur.endOffset, true));
      } else {
        // Handle description of non-textual lines.
        new Output()
            .withRichSpeech(
                new Range(cur.start, cur.end), new Range(prev.start, prev.end),
                Output.EventType.NAVIGATE)
            .go();
      }

      // Be careful to update state in EditableTextBase since we don't
      // explicitly call through to it here.
      this.updateIntraLineState_(cur);

      this.speakAllMarkers_(cur);
      return;
    }

    const curBase = baseLineOnStart ? endLine : startLine;
    if (cur.text === '\u00a0' && cur.hasCollapsedSelection() &&
        !cur.end.node.nextOnLine) {
      // This is a specific pattern seen in Google Docs. A single node (static
      // text/in line text box), containing a non-breaking-space signifies a new
      // line.
      ChromeVox.tts.speak('\n', QueueMode.CATEGORY_FLUSH);
    } else if (
        (cur.startContainer.role === RoleType.TEXT_FIELD ||
         (cur.startContainer === prev.startContainer &&
          cur.endContainer === prev.endContainer)) &&
        cur.startContainerValue !== prev.startContainerValue) {
      // This block catches text changes between |prev| and | cur|. Note that we
      // can end up here if |prevStartLine| or |prevEndLine| were invalid
      // above for intra-line changes. This block therefore catches all text
      // changes including those that occur within a single line and up to those
      // that occur within a static text. It also catches text changes that
      // result in an empty text field, so we handle the case where the
      // container is the text field itself.

      // Take the difference of the text at the paragraph level (i.e. the value
      // of the container) and speak that.
      this.describeTextChanged(
          new TextChangeEvent(
              prev.startContainerValue, prev.localContainerStartOffset,
              prev.localContainerEndOffset, true),
          new TextChangeEvent(
              cur.startContainerValue, cur.localContainerStartOffset,
              cur.localContainerEndOffset, true));
    } else if (cur.text === '') {
      // This line has no text content. Describe the DOM selection.
      new Output()
          .withRichSpeech(
              new Range(cur.start, cur.end), new Range(prev.start, prev.end),
              Output.EventType.NAVIGATE)
          .go();
    } else if (
        !prev.hasCollapsedSelection() && !cur.hasCollapsedSelection() &&
        (curBase.isSameLineAndSelection(prevStartLine) ||
         curBase.isSameLineAndSelection(prevEndLine))) {
      // This is a selection that gets extended from the same anchor.

      // Speech requires many more states than braille.
      const curExtent = baseLineOnStart ? startLine : endLine;
      let text = '';
      let suffixMsg = '';
      if (curBase.isBeforeLine(curExtent)) {
        // Forward selection.
        if (prev.isBeforeLine(curBase)) {
          // Wrapped across the baseline. Read out the new selection.
          suffixMsg = 'selected';
          text = this.getTextSelection_(
              curBase.startContainer, curBase.localStartOffset,
              curExtent.endContainer, curExtent.localEndOffset);
        } else {
          if (prev.isBeforeLine(curExtent)) {
            // Grew.
            suffixMsg = 'selected';
            text = this.getTextSelection_(
                prev.endContainer, prev.localEndOffset, curExtent.endContainer,
                curExtent.localEndOffset);
          } else {
            // Shrank.
            suffixMsg = 'unselected';
            text = this.getTextSelection_(
                curExtent.endContainer, curExtent.localEndOffset,
                prev.endContainer, prev.localEndOffset);
          }
        }
      } else {
        // Backward selection.
        if (curBase.isBeforeLine(prev)) {
          // Wrapped across the baseline. Read out the new selection.
          suffixMsg = 'selected';
          text = this.getTextSelection_(
              curExtent.startContainer, curExtent.localStartOffset,
              curBase.endContainer, curBase.localEndOffset);
        } else {
          if (curExtent.isBeforeLine(prev)) {
            // Grew.
            suffixMsg = 'selected';
            text = this.getTextSelection_(
                curExtent.startContainer, curExtent.localStartOffset,
                prev.startContainer, prev.localStartOffset);
          } else {
            // Shrank.
            suffixMsg = 'unselected';
            text = this.getTextSelection_(
                prev.startContainer, prev.localStartOffset,
                curExtent.startContainer, curExtent.localStartOffset);
          }
        }
      }

      ChromeVox.tts.speak(text, QueueMode.CATEGORY_FLUSH);
      ChromeVox.tts.speak(Msgs.getMsg(suffixMsg), QueueMode.QUEUE);
    } else if (!cur.hasCollapsedSelection()) {
      // Without any other information, try describing the selection. This state
      // catches things like select all.
      const text = this.getTextSelection_(
          cur.startContainer, cur.localStartOffset, cur.endContainer,
          cur.localEndOffset);
      ChromeVox.tts.speak(text, QueueMode.CATEGORY_FLUSH);
      ChromeVox.tts.speak(Msgs.getMsg('selected'), QueueMode.QUEUE);
    } else {
      // A catch-all for any other transitions.

      // Describe the current line. This accounts for previous/current
      // selections and picking the line edge boundary that changed (as computed
      // above). This is also the code path for describing paste. It also covers
      // jump commands which are non-overlapping selections from prev to cur.
      this.line_.speakLine(prev);
    }
    this.updateIntraLineState_(cur);
  }

  /** @private */
  handleBraille_() {
    const isFirstLine = this.isSelectionOnFirstLine();
    const cur = this.line_;
    if (cur.value === null) {
      return;
    }

    let value = new MultiSpannable(cur.value);
    if (!this.node_.constructor) {
      return;
    }
    value.getSpansInstanceOf(this.node_.constructor).forEach(function(span) {
      const style = span.role === RoleType.INLINE_TEXT_BOX ? span.parent : span;
      if (!style) {
        return;
      }
      let formType = FormType.PLAIN_TEXT;
      // Currently no support for sub/superscript in 3rd party liblouis library.
      if (style.bold) {
        formType |= FormType.BOLD;
      }
      if (style.italic) {
        formType |= FormType.ITALIC;
      }
      if (style.underline) {
        formType |= FormType.UNDERLINE;
      }
      if (formType === FormType.PLAIN_TEXT) {
        return;
      }
      const start = value.getSpanStart(span);
      const end = value.getSpanEnd(span);
      value.setSpan(
          new BrailleTextStyleSpan(
              /** @type {LibLouis.FormType<number>} */ (formType)),
          start, end);
    });

    // Provide context for the current selection.
    const context = cur.startContainer;
    if (context && context.role !== RoleType.TEXT_FIELD) {
      const output = new Output().suppress('name').withBraille(
          Range.fromNode(context), Range.fromNode(this.node_),
          Output.EventType.NAVIGATE);
      if (output.braille.length) {
        const end = cur.containerEndOffset + 1;
        const prefix = value.substring(0, end);
        const suffix = value.substring(end, value.length);
        value = prefix;
        value.append(Output.SPACE);
        value.append(output.braille);
        if (suffix.length) {
          if (suffix.toString()[0] !== Output.SPACE) {
            value.append(Output.SPACE);
          }
          value.append(suffix);
        }
      }
    }

    if (isFirstLine) {
      if (!/\s/.test(value.toString()[value.length - 1])) {
        value.append(Output.SPACE);
      }
      value.append(Msgs.getMsg('tag_textarea_brl'));
    }
    value.setSpan(new ValueSpan(0), 0, cur.value.length);
    value.setSpan(new ValueSelectionSpan(), cur.startOffset, cur.endOffset);
    ChromeVox.braille.write(new NavBraille(
        {text: value, startIndex: cur.startOffset, endIndex: cur.endOffset}));
  }

  /**
   * @param {AutomationNode|undefined} startNode
   * @param {number} startOffset
   * @param {AutomationNode|undefined} endNode
   * @param {number} endOffset
   * @return {string}
   */
  getTextSelection_(startNode, startOffset, endNode, endOffset) {
    if (!startNode || !endNode) {
      return '';
    }

    if (startNode === endNode) {
      return startNode.name ? startNode.name.substring(startOffset, endOffset) :
                              '';
    }

    let text = '';
    if (startNode.name) {
      text = startNode.name.substring(startOffset);
    }

    for (let node = startNode;
         (node = AutomationUtil.findNextNode(
              node, Dir.FORWARD, AutomationPredicate.leafOrStaticText)) &&
         node !== endNode;) {
      // Padding needs to get added to break up speech utterances.
      if (node.name) {
        text += ' ' + node.name;
      }
    }

    if (endNode.name) {
      text += ' ' + endNode.name.substring(0, endOffset);
    }
    return text;
  }

  /**
   * @param {AutomationNode!} container
   * @param {number} selStart
   * @param {number} selEnd
   * @private
   */
  speakTextMarker_(container, selStart, selEnd) {
    const markersWithinSelection = {};
    const markers = container.markers;
    if (markers) {
      for (const marker of markers) {
        // See if our selection intersects with this marker.
        if (marker.startOffset >= selStart || selEnd < marker.endOffset) {
          for (const key in marker.flags) {
            markersWithinSelection[key] = true;
          }
        }
      }
    }

    const msgs = [];
    if (this.misspelled ===
        !(markersWithinSelection[chrome.automation.MarkerType.SPELLING])) {
      this.misspelled = !this.misspelled;
      msgs.push(this.misspelled ? 'misspelling_start' : 'misspelling_end');
    }
    if (this.grammarError ===
        !(markersWithinSelection[chrome.automation.MarkerType.GRAMMAR])) {
      this.grammarError = !this.grammarError;
      msgs.push(this.grammarError ? 'grammar_start' : 'grammar_end');
    }

    if (msgs.length) {
      msgs.forEach(function(msg) {
        ChromeVox.tts.speak(
            Msgs.getMsg(msg), QueueMode.QUEUE,
            AbstractTts.PERSONALITY_ANNOTATION);
      });
    }
  }

  /**
   * @param {!AutomationNode} style
   * @private
   */
  speakTextStyle_(style) {
    const msgs = [];
    const fontSize = style.fontSize;
    const fontColor = Color.getColorDescription(style.color);
    const linked = style.state[StateType.LINKED];
    const subscript = style.state.subscript;
    const superscript = style.state.superscript;
    const bold = style.bold;
    const italic = style.italic;
    const underline = style.underline;
    const lineThrough = style.lineThrough;
    const fontFamily = style.fontFamily;

    // Only report text style attributes if they change.
    if (fontSize && (fontSize !== this.fontSize_)) {
      this.fontSize_ = fontSize;
      msgs.push({msg: 'font_size', opt_subs: [this.fontSize_]});
    }
    if (fontColor && (fontColor !== this.fontColor_)) {
      this.fontColor_ = fontColor;
      msgs.push({msg: 'font_color', opt_subs: [this.fontColor_]});
    }
    if (linked !== this.linked_) {
      this.linked_ = linked;
      msgs.push(this.linked_ ? {msg: 'link'} : {msg: 'not_link'});
    }
    if (style.subscript !== this.subscript_) {
      this.subscript_ = subscript;
      msgs.push(this.subscript_ ? {msg: 'subscript'} : {msg: 'not_subscript'});
    }
    if (style.superscript !== this.superscript_) {
      this.superscript_ = superscript;
      msgs.push(
          this.superscript_ ? {msg: 'superscript'} : {msg: 'not_superscript'});
    }
    if (bold !== this.bold_) {
      this.bold_ = bold;
      msgs.push(this.bold_ ? {msg: 'bold'} : {msg: 'not_bold'});
    }
    if (italic !== this.italic_) {
      this.italic_ = italic;
      msgs.push(this.italic_ ? {msg: 'italic'} : {msg: 'not_italic'});
    }
    if (underline !== this.underline_) {
      this.underline_ = underline;
      msgs.push(this.underline_ ? {msg: 'underline'} : {msg: 'not_underline'});
    }
    if (lineThrough !== this.lineThrough_) {
      this.lineThrough_ = lineThrough;
      msgs.push(
          this.lineThrough_ ? {msg: 'linethrough'} : {msg: 'not_linethrough'});
    }
    if (fontFamily && (fontFamily !== this.fontFamily_)) {
      this.fontFamily_ = fontFamily;
      msgs.push({msg: 'font_family', opt_subs: [this.fontFamily_]});
    }

    if (msgs.length) {
      msgs.forEach(function(obj) {
        ChromeVox.tts.speak(
            Msgs.getMsg(obj.msg, obj.opt_subs), QueueMode.QUEUE,
            AbstractTts.PERSONALITY_ANNOTATION);
      });
    }
  }

  /** @override */
  describeSelectionChanged(evt) {
    // Note that since Chrome allows for selection to be placed immediately at
    // the end of a line (i.e. end == value.length) and since we try to describe
    // the character to the right, just describe it as a new line.
    if ((this.start + 1) === evt.start && evt.start === this.value.length) {
      this.speak('\n', evt.triggeredByUser);
      return;
    }

    ChromeVoxEditableTextBase.prototype.describeSelectionChanged.call(
        this, evt);
  }

  /** @override */
  getLineIndex(charIndex) {
    return 0;
  }

  /** @override */
  getLineStart(lineIndex) {
    return 0;
  }

  /** @override */
  getLineEnd(lineIndex) {
    return this.value.length;
  }

  /** @override */
  changed(evt) {
    // This path does not use the Output module to synthesize speech.
    Output.forceModeForNextSpeechUtterance(undefined);
    ChromeVoxEditableTextBase.prototype.changed.call(this, evt);
  }

  /**
   * @private
   * @param {editing.EditableLine} cur Current line.
   */
  updateIntraLineState_(cur) {
    let text = cur.text;
    if (text === '\n') {
      text = '';
    }
    this.value = text;
    this.start = cur.startOffset;
    this.end = cur.endOffset;
  }

  /**
   * @param {!Array<AutomationIntent>} intents
   * @param {!editing.EditableLine} cur
   * @param {!editing.EditableLine} prev
   * @return {boolean}
   * @private
   */
  maybeSpeakUsingIntents_(intents, cur, prev) {
    if (IntentHandler.onIntents(intents, cur, prev)) {
      this.updateIntraLineState_(cur);
      this.speakAllMarkers_(cur);
      return true;
    }

    return false;
  }

  /**
   * @param {!editing.EditableLine} cur
   * @private
   */
  speakAllMarkers_(cur) {
    const container = cur.startContainer;
    if (!container) {
      return;
    }

    this.speakTextMarker_(container, cur.localStartOffset, cur.localEndOffset);

    if (localStorage['announceRichTextAttributes'] === 'true') {
      this.speakTextStyle_(container);
    }
  }
};


/**
 * An observer that reacts to ChromeVox range changes that modifies braille
 * table output when over email or url text fields.
 * @implements {ChromeVoxStateObserver}
 */
editing.EditingChromeVoxStateObserver = class {
  constructor() {
    ChromeVoxState.addObserver(this);
  }

  /** @override */
  onCurrentRangeChanged(range) {
    const inputType = range && range.start.node.inputType;
    if (inputType === 'email' || inputType === 'url') {
      BrailleBackground.getInstance().getTranslatorManager().refresh(
          localStorage['brailleTable8']);
      return;
    }
    BrailleBackground.getInstance().getTranslatorManager().refresh(
        localStorage['brailleTable']);
  }
};


/**
 * @private {ChromeVoxStateObserver}
 */
editing.observer_ = new editing.EditingChromeVoxStateObserver();
});  // goog.scope
