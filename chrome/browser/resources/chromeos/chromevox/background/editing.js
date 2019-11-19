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
goog.require('Output');
goog.require('Output.EventType');
goog.require('TreePathRecoveryStrategy');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('BrailleBackground');
goog.require('ChromeVoxEditableTextBase');
goog.require('LibLouis.FormType');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Cursor = cursors.Cursor;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var FormType = LibLouis.FormType;
var Range = cursors.Range;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;
var Movement = cursors.Movement;
var Unit = cursors.Unit;

/**
 * A handler for automation events in a focused text field or editable root
 * such as a |contenteditable| subtree.
 * @constructor
 * @param {!AutomationNode} node
 */
editing.TextEditHandler = function(node) {
  /** @const {!AutomationNode} @private */
  this.node_ = node;

  if (!node.state[StateType.EDITABLE]) {
    throw '|node| must be editable.';
  }

  chrome.automation.getDesktop(function(desktop) {
    // A rich text field is one where selection gets placed on a DOM descendant
    // to a root text field. This is one of:
    // - content editables (detected via richly editable state)
    // - the node is a textarea
    //
    // The only other editables we expect are all single line (including those
    // from ARC++).
    var useRichText = node.state[StateType.RICHLY_EDITABLE];

    /** @private {!AutomationEditableText} */
    this.editableText_ = useRichText ? new AutomationRichEditableText(node) :
                                       new AutomationEditableText(node);
  }.bind(this));
};

editing.TextEditHandler.prototype = {
  /** @return {!AutomationNode} */
  get node() {
    return this.node_;
  },

  /**
   * Receives the following kinds of events when the node provided to the
   * constructor is focuse: |focus|, |textChanged|, |textSelectionChanged| and
   * |valueChanged|.
   * An implementation of this method should emit the appropriate braille and
   * spoken feedback for the event.
   * @param {!(AutomationEvent|CustomAutomationEvent)} evt
   */
  onEvent: function(evt) {
    if (evt.type !== EventType.TEXT_CHANGED &&
        evt.type !== EventType.TEXT_SELECTION_CHANGED &&
        evt.type !== EventType.DOCUMENT_SELECTION_CHANGED &&
        evt.type !== EventType.VALUE_CHANGED && evt.type !== EventType.FOCUS)
      return;
    if (!evt.target.state.focused || !evt.target.state.editable ||
        evt.target != this.node_)
      return;

    this.editableText_.onUpdate(evt.eventFrom);
  },

  /**
   * Returns true if selection starts at the first line.
   * @return {boolean}
   */
  isSelectionOnFirstLine: function() {
    return this.editableText_.isSelectionOnFirstLine();
  },

  /**
   * Returns true if selection ends at the last line.
   * @return {boolean}
   */
  isSelectionOnLastLine: function() {
    return this.editableText_.isSelectionOnLastLine();
  },

  /**
   * Moves range to after this text field.
   */
  moveToAfterEditText: function() {
    var after = AutomationUtil.findNextNode(
                    this.node_, Dir.FORWARD, AutomationPredicate.object,
                    {skipInitialSubtree: true}) ||
        this.node_;
    ChromeVoxState.instance.navigateToRange(cursors.Range.fromNode(after));
  }
};

/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields.
 * @constructor
 * @param {!AutomationNode} node
 * @extends {ChromeVoxEditableTextBase}
 */
function AutomationEditableText(node) {
  if (!node.state.editable) {
    throw Error('Node must have editable state set to true.');
  }
  var value = this.getProcessedValue_(node) || '';
  /** @private {!Array<number>} */
  this.lineBreaks_ = [];
  this.updateLineBreaks_(value);
  var start = node.textSelStart;
  var end = node.textSelEnd;
  ChromeVoxEditableTextBase.call(
      this, value, Math.min(start, end, value.length),
      Math.min(Math.max(start, end), value.length),
      node.state[StateType.PROTECTED] /**password*/, ChromeVox.tts);
  /** @override */
  this.multiline = node.state[StateType.MULTILINE] || false;
  /** @type {!AutomationNode} @private */
  this.node_ = node;
}

AutomationEditableText.prototype = {
  __proto__: ChromeVoxEditableTextBase.prototype,

  /**
   * Called when the text field has been updated.
   * @param {string|undefined} eventFrom
   */
  onUpdate: function(eventFrom) {
    var oldValue = this.value;
    var oldStart = this.start;
    var oldEnd = this.end;
    var newValue = this.getProcessedValue_(this.node_) || '';
    this.updateLineBreaks_(newValue);

    var textChangeEvent = new TextChangeEvent(
        newValue, Math.min(this.node_.textSelStart || 0, newValue.length),
        Math.min(this.node_.textSelEnd || 0, newValue.length),
        true /* triggered by user */);
    this.changed(textChangeEvent);
    this.outputBraille_(oldValue, oldStart, oldEnd);
  },

  /**
   * Returns true if selection starts on the first line.
   */
  isSelectionOnFirstLine: function() {
    return this.getLineIndex(this.start) == 0;
  },

  /**
   * Returns true if selection ends on the last line.
   */
  isSelectionOnLastLine: function() {
    return this.getLineIndex(this.end) >= this.lineBreaks_.length - 1;
  },

  /** @override */
  getLineIndex: function(charIndex) {
    var lineIndex = 0;
    while (charIndex > this.lineBreaks_[lineIndex])
      lineIndex++;
    return lineIndex;
  },

  /** @override */
  getLineStart: function(lineIndex) {
    if (lineIndex == 0) {
      return 0;
    }

    // The start of this line is defined as the line break of the previous line
    // + 1 (the hard line break).
    return this.lineBreaks_[lineIndex - 1] + 1;
  },

  /** @override */
  getLineEnd: function(lineIndex) {
    return this.lineBreaks_[lineIndex];
  },

  /** @private */
  outputBraille_: function(oldValue, oldStart, oldEnd) {
    var lineIndex = this.getLineIndex(this.start);
    // Output braille at the end of the selection that changed, if start and end
    // differ.
    if (this.start != this.end && this.start == oldStart) {
      lineIndex = this.getLineIndex(this.end);
    }
    var lineStart = this.getLineStart(lineIndex);
    var lineText =
        this.value.substr(lineStart, this.getLineEnd(lineIndex) - lineStart);

    if (lineIndex == 0) {
      lineText += ' ' +
          Msgs.getMsg(this.multiline ? 'tag_textarea_brl' : 'role_textbox_brl');
    }

    ChromeVox.braille.write(new NavBraille({
      text: lineText,
      startIndex: this.start - lineStart,
      endIndex: this.end - lineStart
    }));
  },

  /**
   * @param {!AutomationNode} node
   * @return {string|undefined}
   * @private
   */
  getProcessedValue_: function(node) {
    var value = node.value;
    return (value && node.inputType == 'tel') ? value['trimEnd']() : value;
  },

  /**
   * @private
   */
  updateLineBreaks_: function(value) {
    if (value == this.value) {
      return;
    }

    this.lineBreaks_ = [];
    var lines = value.split('\n');
    for (var i = 0, total = 0; i < lines.length; i++) {
      total += lines[i].length;
      this.lineBreaks_[i] = total;

      // Account for the line break itself.
      total++;
    }
  }
};


/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields using anchor and focus selection.
 * @constructor
 * @param {!AutomationNode} node
 * @extends {AutomationEditableText}
 */
function AutomationRichEditableText(node) {
  AutomationEditableText.call(this, node);

  var root = this.node_.root;
  if (!root || !root.selectionStartObject || !root.selectionEndObject ||
      root.selectionStartOffset === undefined ||
      root.selectionEndOffset === undefined)
    return;

  this.startLine_ = new editing.EditableLine(
      root.selectionStartObject, root.selectionStartOffset,
      root.selectionStartObject, root.selectionStartOffset);
  this.endLine_ = new editing.EditableLine(
      root.selectionEndObject, root.selectionEndOffset, root.selectionEndObject,
      root.selectionEndOffset);

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

AutomationRichEditableText.prototype = {
  __proto__: AutomationEditableText.prototype,

  /** @override */
  isSelectionOnFirstLine: function() {
    var deep = this.line_.end_.node;
    while (deep.previousOnLine)
      deep = deep.previousOnLine;
    var next = AutomationUtil.findNextNode(
        deep, Dir.BACKWARD, AutomationPredicate.inlineTextBox);
    if (!next) {
      return true;
    }
    var exited = AutomationUtil.getUniqueAncestors(next, deep);
    return !!exited.find(function(item) {
      return item == this.node_;
    }.bind(this));
  },

  /** @override */
  isSelectionOnLastLine: function() {
    var deep = this.line_.end_.node;
    while (deep.nextOnLine)
      deep = deep.nextOnLine;
    var next = AutomationUtil.findNextNode(
        deep, Dir.FORWARD, AutomationPredicate.inlineTextBox);
    if (!next) {
      return true;
    }
    var exited = AutomationUtil.getUniqueAncestors(next, deep);
    return !!exited.find(function(item) {
      return item == this.node_;
    }.bind(this));
  },

  /** @override */
  onUpdate: function(eventFrom) {
    var root = this.node_.root;
    if (!root.selectionStartObject || !root.selectionEndObject ||
        root.selectionStartOffset === undefined ||
        root.selectionEndOffset === undefined)
      return;

    var startLine = new editing.EditableLine(
        root.selectionStartObject, root.selectionStartOffset,
        root.selectionStartObject, root.selectionStartOffset);
    var endLine = new editing.EditableLine(
        root.selectionEndObject, root.selectionEndOffset,
        root.selectionEndObject, root.selectionEndOffset);

    var prevStartLine = this.startLine_;
    var prevEndLine = this.endLine_;
    this.startLine_ = startLine;
    this.endLine_ = endLine;

    var baseLineOnStart = prevEndLine.isSameLineAndSelection(endLine);
    var isSameSelection =
        baseLineOnStart && prevStartLine.isSameLineAndSelection(startLine);

    var cur;
    if (isSameSelection && this.line_) {
      // Nothing changed, return.
      return;
    } else {
      cur = new editing.EditableLine(
          root.selectionStartObject, root.selectionStartOffset,
          root.selectionEndObject, root.selectionEndOffset, baseLineOnStart);
    }
    var prev = this.line_;
    this.line_ = cur;

    // During continuous read, skip speech (which gets handled in
    // CommandHandler). We use the speech end callback to trigger additional
    // speech.
    // Also, skip speech based on the predicate.
    if (ChromeVoxState.isReadingContinuously ||
        AutomationPredicate.shouldOnlyOutputSelectionChangeInBraille(
            this.node_)) {
      this.brailleCurrentRichLine_();
      this.updateIntraLineState_(cur);
      return;
    }

    // Selection stayed within the same line(s) and didn't cross into new lines.

    // We must validate the previous lines as state changes in the accessibility
    // tree may have invalidated the lines.
    if (startLine.isSameLine(prevStartLine) &&
        endLine.isSameLine(prevEndLine)) {
      // Intra-line changes.

      if (cur.hasTextSelection()) {
        var text = cur.text;
        if (text == '\n') {
          text = '';
        }
        this.changed(
            new TextChangeEvent(text, cur.startOffset, cur.endOffset, true));
      } else {
        // Handle description of non-textual lines.
        new Output()
            .withRichSpeech(
                new Range(cur.start_, cur.end_),
                new Range(prev.start_, prev.end_), Output.EventType.NAVIGATE)
            .go();
      }
      this.brailleCurrentRichLine_();

      // Finally, queue up any text markers/styles at bounds.
      var container = cur.startContainer_;
      if (!container) {
        return;
      }

      this.speakTextMarker_(
          container, cur.localStartOffset, cur.localEndOffset);

      if (localStorage['announceRichTextAttributes'] == 'true') {
        this.speakTextStyle_(container);
      }
      return;
    }

    var curBase = baseLineOnStart ? endLine : startLine;
    if ((cur.startContainer_.role == RoleType.TEXT_FIELD ||
         (cur.startContainer_ == prev.startContainer_ &&
          cur.endContainer_ == prev.endContainer_)) &&
        cur.startContainerValue_ != prev.startContainerValue_) {
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
              prev.startContainerValue_, prev.localContainerStartOffset_,
              prev.localContainerEndOffset_, true),
          new TextChangeEvent(
              cur.startContainerValue_, cur.localContainerStartOffset_,
              cur.localContainerEndOffset_, true));

      // Braille here simply displays the current line.
      this.brailleCurrentRichLine_();
    } else if (cur.text == '') {
      // This line has no text content. Describe the DOM selection.
      new Output()
          .withRichSpeechAndBraille(
              new Range(cur.start_, cur.end_),
              new Range(prev.start_, prev.end_), Output.EventType.NAVIGATE)
          .go();
    } else if (
        !cur.hasCollapsedSelection() &&
        (curBase.isSameLine(prevStartLine) ||
         curBase.isSameLine(prevEndLine))) {
      // This is a selection that gets extended from the same anchor.

      // Speech requires many more states than braille.
      var curExtent = baseLineOnStart ? startLine : endLine;
      var text = '';
      var suffixMsg = '';
      if (curBase.isBeforeLine(curExtent)) {
        // Forward selection.
        if (prev.isBeforeLine(curBase)) {
          // Wrapped across the baseline. Read out the new selection.
          suffixMsg = 'selected';
          text = this.getTextSelection_(
              curBase.startContainer_, curBase.localStartOffset,
              curExtent.endContainer_, curExtent.localEndOffset);
        } else {
          if (prev.isBeforeLine(curExtent)) {
            // Grew.
            suffixMsg = 'selected';
            text = this.getTextSelection_(
                prev.endContainer_, prev.localEndOffset,
                curExtent.endContainer_, curExtent.localEndOffset);
          } else {
            // Shrank.
            suffixMsg = 'unselected';
            text = this.getTextSelection_(
                curExtent.endContainer_, curExtent.localEndOffset,
                prev.endContainer_, prev.localEndOffset);
          }
        }
      } else {
        // Backward selection.
        if (curBase.isBeforeLine(prev)) {
          // Wrapped across the baseline. Read out the new selection.
          suffixMsg = 'selected';
          text = this.getTextSelection_(
              curExtent.startContainer_, curExtent.localStartOffset,
              curBase.endContainer_, curBase.localEndOffset);
        } else {
          if (curExtent.isBeforeLine(prev)) {
            // Grew.
            suffixMsg = 'selected';
            text = this.getTextSelection_(
                curExtent.startContainer_, curExtent.localStartOffset,
                prev.startContainer_, prev.localStartOffset);
          } else {
            // Shrank.
            suffixMsg = 'unselected';
            text = this.getTextSelection_(
                prev.startContainer_, prev.localStartOffset,
                curExtent.startContainer_, curExtent.localStartOffset);
          }
        }
      }

      ChromeVox.tts.speak(text, QueueMode.CATEGORY_FLUSH);
      ChromeVox.tts.speak(Msgs.getMsg(suffixMsg), QueueMode.QUEUE);
      this.brailleCurrentRichLine_();
    } else {
      // A catch-all for any other transitions.

      // Describe the current line. This accounts for previous/current
      // selections and picking the line edge boundary that changed (as computed
      // above). This is also the code path for describing paste. It also covers
      // jump commands which are non-overlapping selections from prev to cur.
      this.speakCurrentRichLine_(prev);
      this.brailleCurrentRichLine_();
    }
    this.updateIntraLineState_(cur);
  },

  /**
   * @param {AutomationNode|undefined} startNode
   * @param {number} startOffset
   * @param {AutomationNode|undefined} endNode
   * @param {number} endOffset
   * @return {string}
   */
  getTextSelection_: function(startNode, startOffset, endNode, endOffset) {
    if (!startNode || !endNode) {
      return '';
    }

    if (startNode == endNode) {
      return startNode.name ? startNode.name.substring(startOffset, endOffset) :
                              '';
    }

    var text = '';
    if (startNode.name) {
      text = startNode.name.substring(startOffset);
    }

    for (var node = startNode;
         (node = AutomationUtil.findNextNode(
              node, Dir.FORWARD, AutomationPredicate.leafOrStaticText)) &&
         node != endNode;) {
      // Padding needs to get added to break up speech utterances.
      if (node.name) {
        text += ' ' + node.name;
      }
    }

    if (endNode.name) {
      text += ' ' + endNode.name.substring(0, endOffset);
    }
    return text;
  },

  /**
   * @param {AutomationNode!} container
   * @param {number} selStart
   * @param {number} selEnd
   * @private
   */
  speakTextMarker_: function(container, selStart, selEnd) {
    var markersWithinSelection = 0;
    if (container.markerTypes) {
      for (var i = 0; i < container.markerTypes.length; i++) {
        // See if our selection intersects with this marker.
        if (container.markerStarts[i] >= selStart ||
            selEnd < container.markerEnds[i])
          markersWithinSelection |= container.markerTypes[i];
      }
    }

    var msgs = [];
    if (this.misspelled == !(markersWithinSelection & 1)) {
      this.misspelled = !this.misspelled;
      msgs.push(this.misspelled ? 'misspelling_start' : 'misspelling_end');
    }
    if (this.grammarError == !(markersWithinSelection & 2)) {
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
  },

  /**
   * @param {!AutomationNode} style
   * @private
   */
  speakTextStyle_: function(style) {
    var msgs = [];
    var fontSize = style.fontSize;
    var fontColor = Color.getColorDescription(style.color);
    var linked = style.state[StateType.LINKED];
    var subscript = style.state.subscript;
    var superscript = style.state.superscript;
    var bold = style.bold;
    var italic = style.italic;
    var underline = style.underline;
    var lineThrough = style.lineThrough;
    var fontFamily = style.fontFamily;

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
  },

  /**
   * @param {editing.EditableLine} prevLine
   * @private
   */
  speakCurrentRichLine_: function(prevLine) {
    var prev = (prevLine && prevLine.startContainer_.role) ?
        prevLine.startContainer_ :
        null;
    var lineNodes =
        /** @type {Array<!AutomationNode>} */ (
            this.line_.value_.getSpansInstanceOf(
                /** @type {function()} */ (this.node_.constructor)));
    var queueMode = QueueMode.CATEGORY_FLUSH;
    for (var i = 0, cur; cur = lineNodes[i]; i++) {
      if (cur.children.length) {
        continue;
      }

      var o = new Output()
                  .withRichSpeech(
                      Range.fromNode(cur),
                      prev ? Range.fromNode(prev) : Range.fromNode(cur),
                      Output.EventType.NAVIGATE)
                  .withQueueMode(queueMode);

      // Ignore whitespace only output except if it is leading content on the
      // line.
      if (!o.isOnlyWhitespace || i == 0) {
        o.go();
      }
      prev = cur;
      queueMode = QueueMode.QUEUE;
    }
  },

  /** @private */
  brailleCurrentRichLine_: function() {
    var isFirstLine = this.isSelectionOnFirstLine();
    var cur = this.line_;
    if (cur.value_ === null) {
      return;
    }

    var value = new MultiSpannable(cur.value_);
    if (!this.node_.constructor) {
      return;
    }
    value.getSpansInstanceOf(this.node_.constructor).forEach(function(span) {
      var style = span.role == RoleType.INLINE_TEXT_BOX ? span.parent : span;
      if (!style) {
        return;
      }
      var formType = FormType.PLAIN_TEXT;
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
      if (formType == FormType.PLAIN_TEXT) {
        return;
      }
      var start = value.getSpanStart(span);
      var end = value.getSpanEnd(span);
      value.setSpan(
          new BrailleTextStyleSpan(
              /** @type {LibLouis.FormType<number>} */ (formType)),
          start, end);
    });

    // Provide context for the current selection.
    var context = cur.startContainer_;

    if (context) {
      var output = new Output().suppress('name').withBraille(
          Range.fromNode(context), Range.fromNode(this.node_),
          Output.EventType.NAVIGATE);
      if (output.braille.length) {
        var end = cur.containerEndOffset + 1;
        var prefix = value.substring(0, end);
        var suffix = value.substring(end, value.length);
        value = prefix;
        value.append(Output.SPACE);
        value.append(output.braille);
        if (suffix.length) {
          if (suffix.toString()[0] != Output.SPACE) {
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
    value.setSpan(new ValueSpan(0), 0, cur.value_.length);
    value.setSpan(new ValueSelectionSpan(), cur.startOffset, cur.endOffset);
    ChromeVox.braille.write(new NavBraille(
        {text: value, startIndex: cur.startOffset, endIndex: cur.endOffset}));
  },

  /** @override */
  describeSelectionChanged: function(evt) {
    // Note that since Chrome allows for selection to be placed immediately at
    // the end of a line (i.e. end == value.length) and since we try to describe
    // the character to the right, just describe it as a new line.
    if ((this.start + 1) == evt.start && evt.start == this.value.length) {
      this.speak('\n', evt.triggeredByUser);
      return;
    }

    ChromeVoxEditableTextBase.prototype.describeSelectionChanged.call(
        this, evt);
  },

  /** @override */
  getLineIndex: function(charIndex) {
    return 0;
  },

  /** @override */
  getLineStart: function(lineIndex) {
    return 0;
  },

  /** @override */
  getLineEnd: function(lineIndex) {
    return this.value.length;
  },

  /** @override */
  changed: function(evt) {
    // This path does not use the Output module to synthesize speech.
    Output.forceModeForNextSpeechUtterance(undefined);
    ChromeVoxEditableTextBase.prototype.changed.call(this, evt);
  },

  /**
   * @private
   * @param {editing.EditableLine} cur Current line.
   */
  updateIntraLineState_: function(cur) {
    this.value = cur.text;
    this.start = cur.startOffset;
    this.end = cur.endOffset;
  }
};

/**
 * @param {!AutomationNode} node The root editable node, i.e. the root of a
 *     contenteditable subtree or a text field.
 * @return {editing.TextEditHandler}
 */
editing.TextEditHandler.createForNode = function(node) {
  if (!node.state.editable) {
    throw new Error('Expected editable node.');
  }

  return new editing.TextEditHandler(node);
};

/**
 * An observer that reacts to ChromeVox range changes that modifies braille
 * table output when over email or url text fields.
 * @constructor
 * @implements {ChromeVoxStateObserver}
 */
editing.EditingChromeVoxStateObserver = function() {
  ChromeVoxState.addObserver(this);
};

editing.EditingChromeVoxStateObserver.prototype = {
  __proto__: ChromeVoxStateObserver,

  /** @override */
  onCurrentRangeChanged: function(range) {
    var inputType = range && range.start.node.inputType;
    if (inputType == 'email' || inputType == 'url') {
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

/**
 * An EditableLine encapsulates all data concerning a line in the automation
 * tree necessary to provide output.
 * Editable: an editable selection (e.g. start/end offsets) get saved.
 * Line: nodes/offsets at the beginning/end of a line get saved.
 * @param {!AutomationNode} startNode
 * @param {number} startIndex
 * @param {!AutomationNode} endNode
 * @param {number} endIndex
 * @param {boolean=} opt_baseLineOnStart  Controls whether to use
 *     |startNode| or |endNode| for Line computations. Selections are
 * automatically truncated up to either the line start or end.
 * @constructor
 */
editing.EditableLine = function(
    startNode, startIndex, endNode, endIndex, opt_baseLineOnStart) {
  /** @private {!Cursor} */
  this.start_ = new Cursor(startNode, startIndex);
  this.start_ = this.start_.deepEquivalent || this.start_;
  /** @private {!Cursor} */
  this.end_ = new Cursor(endNode, endIndex);
  this.end_ = this.end_.deepEquivalent || this.end_;

  // Update |startIndex| and |endIndex| if the calls above to
  // cursors.Cursor.deepEquivalent results in cursors to different container
  // nodes. The cursors can point directly to inline text boxes, in which case
  // we should not adjust the container start or end index.
  if (startNode.role == RoleType.STATIC_TEXT && this.start_.node != startNode &&
      this.start_.node.parent != startNode) {
    startIndex = this.start_.index == cursors.NODE_INDEX ?
        this.start_.node.name.length :
        this.start_.index;
  }
  if (endNode.role == RoleType.STATIC_TEXT && this.end_.node != endNode &&
      this.end_.node.parent != endNode) {
    endIndex = this.end_.index == cursors.NODE_INDEX ?
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
};

editing.EditableLine.prototype = {
  /** @private */
  computeLineData_: function(opt_baseLineOnStart) {
    // Note that we calculate the line based only upon |start_| or
    // |end_| even if they do not fall on the same line. It is up to
    // the caller to specify which end to base this line upon since it requires
    // reasoning about two lines.
    var nameLen = 0;
    var lineBase = opt_baseLineOnStart ? this.start_ : this.end_;
    var lineExtend = opt_baseLineOnStart ? this.end_ : this.start_;

    if (lineBase.node.name) {
      nameLen = lineBase.node.name.length;
    }

    this.value_ = new Spannable(lineBase.node.name || '', lineBase);
    if (lineBase.node == lineExtend.node) {
      this.value_.setSpan(lineExtend, 0, nameLen);
    }

    this.startContainer_ = this.start_.node;
    if (this.startContainer_.role == RoleType.INLINE_TEXT_BOX) {
      this.startContainer_ = this.startContainer_.parent;
    }
    this.startContainerValue_ =
        this.startContainer_.role == RoleType.TEXT_FIELD ?
        this.startContainer_.value || '' :
        this.startContainer_.name || '';
    this.endContainer_ = this.end_.node;
    if (this.endContainer_.role == RoleType.INLINE_TEXT_BOX) {
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
    var parents = [this.startContainer_];

    // Compute the start of line.
    var lineStart = this.lineStart_;

    // Hack: note underlying bugs require these hacks.
    while ((lineStart.previousOnLine && lineStart.previousOnLine.role) ||
           (lineStart.previousSibling && lineStart.previousSibling.lastChild &&
            lineStart.previousSibling.lastChild.nextOnLine == lineStart)) {
      if (lineStart.previousOnLine) {
        lineStart = lineStart.previousOnLine;
      } else {
        lineStart = lineStart.previousSibling.lastChild;
      }

      this.lineStart_ = lineStart;

      if (lineStart.role != RoleType.INLINE_TEXT_BOX) {
        parents.unshift(lineStart);
      } else if (parents[0] != lineStart.parent) {
        parents.unshift(lineStart.parent);
      }

      var prepend = new Spannable(lineStart.name, lineStart);
      prepend.append(this.value_);
      this.value_ = prepend;
    }
    this.lineStartContainer_ = this.lineStart_.parent;

    var lineEnd = this.lineEnd_;

    // Hack: note underlying bugs require these hacks.
    while ((lineEnd.nextOnLine && lineEnd.nextOnLine.role) ||
           (lineEnd.nextSibling &&
            lineEnd.nextSibling.previousOnLine == lineEnd)) {
      if (lineEnd.nextOnLine) {
        lineEnd = lineEnd.nextOnLine;
      } else {
        lineEnd = lineEnd.nextSibling.firstChild;
      }

      this.lineEnd_ = lineEnd;

      if (lineEnd.role != RoleType.INLINE_TEXT_BOX) {
        parents.push(this.lineEnd_);
      } else if (parents[parents.length - 1] != lineEnd.parent) {
        parents.push(this.lineEnd_.parent);
      }

      var annotation = lineEnd;
      if (lineEnd == this.end_.node) {
        annotation = this.end_;
      }

      this.value_.append(new Spannable(lineEnd.name, annotation));
    }
    this.lineEndContainer_ = this.lineEnd_.parent;

    // Finally, annotate with all parent static texts as NodeSpan's so that
    // braille routing can key properly into the node with an offset.
    // Note that both line start and end needs to account for
    // potential offsets into the static texts as follows.
    var textCountBeforeLineStart = 0, textCountAfterLineEnd = 0;
    var finder = this.lineStart_;
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

    var len = 0;
    for (var i = 0; i < parents.length; i++) {
      var parent = parents[i];

      if (!parent.name) {
        continue;
      }

      var prevLen = len;

      var currentLen = parent.name.length;
      var offset = 0;

      // Subtract off the text count before when at the start of line.
      if (i == 0) {
        currentLen -= textCountBeforeLineStart;
        offset = textCountBeforeLineStart;
      }

      // Subtract text count after when at the end of the line.
      if (i == parents.length - 1) {
        currentLen -= textCountAfterLineEnd;
      }

      len += currentLen;

      try {
        this.value_.setSpan(new Output.NodeSpan(parent, offset), prevLen, len);

        // Also, annotate this span if it is associated with line containre.
        if (parent == this.startContainer_) {
          this.value_.setSpan(parent, prevLen, len);
        }
      } catch (e) {
      }
    }
  },

  /**
   * Gets the selection offset based on the text content of this line.
   * @return {number}
   */
  get startOffset() {
    // It is possible that the start cursor points to content before this line
    // (e.g. in a multi-line selection).
    try {
      return this.value_.getSpanStart(this.start_) +
          (this.start_.index == cursors.NODE_INDEX ? 0 : this.start_.index);
    } catch (e) {
      // When that happens, fall back to the start of this line.
      return 0;
    }
  },

  /**
   * Gets the selection offset based on the text content of this line.
   * @return {number}
   */
  get endOffset() {
    try {
      return this.value_.getSpanStart(this.end_) +
          (this.end_.index == cursors.NODE_INDEX ? 0 : this.end_.index);
    } catch (e) {
      return this.value_.length;
    }
  },

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   * @return {number}
   */
  get localStartOffset() {
    return this.localContainerStartOffset_;
  },

  /**
   * Gets the selection offset based on the parent's text.
   * The parent is expected to be static text.
   * @return {number}
   */
  get localEndOffset() {
    return this.localContainerEndOffset_;
  },

  /**
   * Gets the start offset of the container, relative to the line text content.
   * The container refers to the static text parenting the inline text box.
   * @return {number}
   */
  get containerStartOffset() {
    return this.value_.getSpanStart(this.startContainer_);
  },

  /**
   * Gets the end offset of the container, relative to the line text content.
   * The container refers to the static text parenting the inline text box.
   * @return {number}
   */
  get containerEndOffset() {
    return this.value_.getSpanEnd(this.startContainer_) - 1;
  },

  /**
   * The text content of this line.
   * @return {string} The text of this line.
   */
  get text() {
    return this.value_.toString();
  },

  /** @return {string} */
  get selectedText() {
    return this.value_.toString().substring(this.startOffset, this.endOffset);
  },

  /** @return {boolean} */
  hasCollapsedSelection: function() {
    return this.start_.equals(this.end_);
  },

  /**
   * Returns whether this line has selection over text nodes.
   */
  hasTextSelection() {
    if (this.start_.node && this.end_.node) {
      return AutomationPredicate.text(this.start_.node) &&
          AutomationPredicate.text(this.end_.node);
    }
  },

  /**
   * Returns true if |otherLine| surrounds the same line as |this|. Note that
   * the contents of the line might be different.
   * @param {editing.EditableLine} otherLine
   * @return {boolean}
   */
  isSameLine: function(otherLine) {
    // Equality is intentionally loose here as any of the state nodes can be
    // invalidated at any time. We rely upon the start/anchor of the line
    // staying the same.
    return (otherLine.lineStartContainer_ == this.lineStartContainer_ &&
            otherLine.localLineStartContainerOffset_ ==
                this.localLineStartContainerOffset_) ||
        (otherLine.lineEndContainer_ == this.lineEndContainer_ &&
         otherLine.localLineEndContainerOffset_ ==
             this.localLineEndContainerOffset_) ||
        (otherLine.lineStartContainerRecovery_.node ==
             this.lineStartContainerRecovery_.node &&
         otherLine.localLineStartContainerOffset_ ==
             this.localLineStartContainerOffset_);
  },

  /**
   * Returns true if |otherLine| surrounds the same line as |this| and has the
   * same selection.
   * @param {editing.EditableLine} otherLine
   * @return {boolean}
   */
  isSameLineAndSelection: function(otherLine) {
    return this.isSameLine(otherLine) &&
        this.startOffset == otherLine.startOffset &&
        this.endOffset == otherLine.endOffset;
  },

  /**
   * Returns whether this line comes before |otherLine| in document order.
   * @return {boolean}
   */
  isBeforeLine: function(otherLine) {
    if (this.isSameLine(otherLine) || !this.lineStartContainer_ ||
        !otherLine.lineStartContainer_)
      return false;
    return AutomationUtil.getDirection(
               this.lineStartContainer_, otherLine.lineStartContainer_) ==
        Dir.FORWARD;
  },

  /**
   * Performs a validation that this line still refers to a line given its
   * internally tracked state.
   */
  isValidLine: function() {
    if (!this.lineStartContainer_ || !this.lineEndContainer_) {
      return false;
    }

    var start = new cursors.Cursor(
        this.lineStartContainer_, this.localLineStartContainerOffset_);
    var end = new cursors.Cursor(
        this.lineEndContainer_, this.localLineEndContainerOffset_ - 1);
    var localStart = start.deepEquivalent || start;
    var localEnd = end.deepEquivalent || end;
    var localStartNode = localStart.node;
    var localEndNode = localEnd.node;

    // Unfortunately, there are asymmetric errors in lines, so we need to check
    // in both directions.
    var testStartNode = localStartNode;
    do {
      if (testStartNode == localEndNode) {
        return true;
      }

      // Hack/workaround for broken *OnLine links.
      if (testStartNode.nextOnLine && testStartNode.nextOnLine.role) {
        testStartNode = testStartNode.nextOnLine;
      } else if (
          testStartNode.nextSibling &&
          testStartNode.nextSibling.previousOnLine == testStartNode) {
        testStartNode = testStartNode.nextSibling;
      } else {
        break;
      }
    } while (testStartNode);

    var testEndNode = localEndNode;
    do {
      if (testEndNode == localStartNode) {
        return true;
      }

      // Hack/workaround for broken *OnLine links.
      if (testEndNode.previousOnLine && testEndNode.previousOnLine.role) {
        testEndNode = testEndNode.previousOnLine;
      } else if (
          testEndNode.previousSibling &&
          testEndNode.previousSibling.nextOnLine == testEndNode) {
        testEndNode = testEndNode.previousSibling;
      } else {
        break;
      }
    } while (testEndNode);

    return false;
  }
};
});
