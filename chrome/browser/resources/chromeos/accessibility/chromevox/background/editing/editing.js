// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Processes events related to editing text and emits the
 * appropriate spoken and braille feedback.
 */
import {AutomationPredicate} from '../../../common/automation_predicate.js';
import {AutomationUtil} from '../../../common/automation_util.js';
import {constants} from '../../../common/constants.js';
import {Cursor} from '../../../common/cursors/cursor.js';
import {CursorRange} from '../../../common/cursors/range.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {Msgs} from '../../common/msgs.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {MultiSpannable, Spannable} from '../../common/spannable.js';
import {Personality, QueueMode} from '../../common/tts_types.js';
import {BrailleTranslatorManager} from '../braille/braille_translator_manager.js';
import {LibLouis} from '../braille/liblouis.js';
import {BrailleTextStyleSpan, ValueSelectionSpan, ValueSpan} from '../braille/spans.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from '../chromevox_range.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {Color} from '../color.js';
import {Output} from '../output/output.js';
import {OutputCustomEvent} from '../output/output_types.js';

import {EditableLine} from './editable_line.js';
import {AutomationEditableText} from './editable_text.js';
import {ChromeVoxEditableTextBase, TextChangeEvent} from './editable_text_base.js';
import {IntentHandler} from './intent_handler.js';
import {TextEditHandler} from './text_edit_handler.js';

const AutomationIntent = chrome.automation.AutomationIntent;
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const FormType = LibLouis.FormType;
const IntentCommandType = chrome.automation.IntentCommandType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields using anchor and focus selection.
 */
export class AutomationRichEditableText extends AutomationEditableText {
  /** @param {!AutomationNode} node */
  constructor(node) {
    super(node);

    const root = this.node_.root;
    if (!root || !root.selectionStartObject || !root.selectionEndObject ||
        root.selectionStartOffset === undefined ||
        root.selectionEndOffset === undefined) {
      return;
    }

    this.startLine_ = new EditableLine(
        root.selectionStartObject, root.selectionStartOffset,
        root.selectionStartObject, root.selectionStartOffset);
    this.endLine_ = new EditableLine(
        root.selectionEndObject, root.selectionEndOffset,
        root.selectionEndObject, root.selectionEndOffset);

    this.line_ = new EditableLine(
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
        deep, Dir.BACKWARD, AutomationPredicate.inlineTextBox,
        {root: r => r === this.node_});
    if (!next) {
      return true;
    }
    const exited = AutomationUtil.getUniqueAncestors(next, deep);
    return exited.includes(this.node_);
  }

  /** @override */
  isSelectionOnLastLine() {
    let deep = this.line_.end.node;
    while (deep.nextOnLine) {
      deep = deep.nextOnLine;
    }
    const next = AutomationUtil.findNextNode(
        deep, Dir.FORWARD, AutomationPredicate.inlineTextBox,
        {root: r => r === this.node_});
    if (!next) {
      return true;
    }
    const exited = AutomationUtil.getUniqueAncestors(next, deep);
    return exited.includes(this.node_);
  }

  /** @override */
  onUpdate(intents) {
    const root = this.node_.root;
    if (!root.selectionStartObject || !root.selectionEndObject ||
        root.selectionStartOffset === undefined ||
        root.selectionEndOffset === undefined) {
      return;
    }

    const startLine = new EditableLine(
        root.selectionStartObject, root.selectionStartOffset,
        root.selectionStartObject, root.selectionStartOffset);
    const endLine = new EditableLine(
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
      cur = new EditableLine(
          root.selectionStartObject, root.selectionStartOffset,
          root.selectionEndObject, root.selectionEndOffset, baseLineOnStart);
    }
    const prev = this.line_;
    this.line_ = cur;

    this.handleSpeech_(
        cur, prev, startLine, endLine, prevStartLine, prevEndLine,
        baseLineOnStart, intents);
    this.handleBraille_(baseLineOnStart);
  }

  /**
   * @param {!EditableLine} cur
   * @param {!EditableLine} prev
   * @param {!EditableLine} startLine
   * @param {!EditableLine} endLine
   * @param {!EditableLine} prevStartLine
   * @param {!EditableLine} prevEndLine
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
    if (ChromeVoxState.instance.isReadingContinuously ||
        AutomationPredicate.shouldOnlyOutputSelectionChangeInBraille(
            this.node_)) {
      this.updateIntraLineState_(cur);
      return;
    }

    // End of document announcements are special because it's the only situation
    // in which there's no more content to the right of the  on the last
    // linecursor. This condition has to detect a precise state change where a
    // user moves (not changes) within the last line.
    if (this.isSelectionOnLastLine() && cur.hasCollapsedSelection() &&
        cur.text.length === cur.endOffset && prev.isSameLine(cur) &&
        cur.text === prev.text) {
      // Omit announcements if the document is completely empty.
      if (!this.isSelectionOnFirstLine() || cur.text.length > 0) {
        ChromeVox.tts.speak(
            Msgs.getMsg('end_of_text_verbose'), QueueMode.CATEGORY_FLUSH);
      }
      this.updateIntraLineState_(cur);
      return;
    }

    // Before entering into our state machine below, use selected intents to
    // decipher ambiguous cases.
    if (this.maybeSpeakUsingIntents_(intents, cur, prev)) {
      return;
    }

    // We must validate the previous lines below as state changes in the
    // accessibility tree may have invalidated the lines.

    // Selection stayed within the same line(s) and didn't cross into new lines.
    // Handle speech output for collapsed selections and all selections on text
    // areas using EditableTextBase.
    // TODO(accessibility): eventually remove usage of the EditableTextBase
    // plain text state machine by migrating all cases to be handled by
    // EditableLine.
    if ((cur.hasCollapsedSelection() || this.node_.htmlTag === 'textarea') &&
        startLine.isSameLine(prevStartLine) &&
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
                new CursorRange(cur.start, cur.end),
                new CursorRange(prev.start, prev.end),
                OutputCustomEvent.NAVIGATE)
            .go();
      }

      // Be careful to update state in EditableTextBase since we don't
      // explicitly call through to it here.
      this.updateIntraLineState_(cur);

      this.speakAllMarkers_(cur);
      return;
    }

    const curBase = baseLineOnStart ? endLine : startLine;
    if ((cur.startContainer.role === RoleType.TEXT_FIELD ||
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
              new CursorRange(cur.start, cur.end),
              new CursorRange(prev.start, prev.end), OutputCustomEvent.NAVIGATE)
          .go();
    } else if (
        !prev.hasCollapsedSelection() && !cur.hasCollapsedSelection() &&
        (curBase.isSameLineAndSelection(prevStartLine) ||
         curBase.isSameLineAndSelection(prevEndLine))) {
      // This is a selection that gets extended from the same anchor.

      // Speech requires many more states than braille.
      const curExtent = baseLineOnStart ? startLine : endLine;
      let suffixMsg = '';
      if (curBase.isBeforeLine(curExtent)) {
        // Forward selection.
        if (prev.isBeforeLine(curBase) && !prev.start.equals(curBase.start)) {
          // Wrapped across the baseline. Read out the new selection.
          suffixMsg = 'selected';
          this.speakTextSelection_(
              curBase.startContainer, curBase.localStartOffset,
              curExtent.endContainer, curExtent.localEndOffset);
        } else {
          if (prev.isBeforeLine(curExtent)) {
            // Grew.
            suffixMsg = 'selected';
            this.speakTextSelection_(
                prev.endContainer, prev.localEndOffset, curExtent.endContainer,
                curExtent.localEndOffset);
          } else {
            // Shrank.
            suffixMsg = 'unselected';
            this.speakTextSelection_(
                curExtent.endContainer, curExtent.localEndOffset,
                prev.endContainer, prev.localEndOffset);
          }
        }
      } else {
        // Backward selection.
        if (curBase.isBeforeLine(prev)) {
          // Wrapped across the baseline. Read out the new selection.
          suffixMsg = 'selected';
          this.speakTextSelection_(
              curExtent.startContainer, curExtent.localStartOffset,
              curBase.endContainer, curBase.localEndOffset);
        } else {
          if (curExtent.isBeforeLine(prev)) {
            // Grew.
            suffixMsg = 'selected';
            this.speakTextSelection_(
                curExtent.startContainer, curExtent.localStartOffset,
                prev.startContainer, prev.localStartOffset);
          } else {
            // Shrank.
            suffixMsg = 'unselected';
            this.speakTextSelection_(
                prev.startContainer, prev.localStartOffset,
                curExtent.startContainer, curExtent.localStartOffset);
          }
        }
      }

      ChromeVox.tts.speak(Msgs.getMsg(suffixMsg), QueueMode.QUEUE);
    } else if (!cur.hasCollapsedSelection()) {
      // Without any other information, try describing the selection. This state
      // catches things like select all.
      this.speakTextSelection_(
          cur.startContainer, cur.localStartOffset, cur.endContainer,
          cur.localEndOffset);
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

  /**
   * @param {boolean} baseLineOnStart When true, the brailled line will show
   *     ancestry context based on the start of the selection. When false, it
   *     will use the end of the selection.
   * @private
   */
  handleBraille_(baseLineOnStart) {
    const isEmpty = !this.node_.find({role: RoleType.STATIC_TEXT});
    const isFirstLine = this.isSelectionOnFirstLine();
    const cur = this.line_;
    if (cur.value === null) {
      return;
    }

    let value = new MultiSpannable(isEmpty ? '' : cur.value);
    if (!this.node_.constructor) {
      return;
    }
    value.getSpansInstanceOf(this.node_.constructor).forEach(span => {
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

    value.setSpan(new ValueSpan(0), 0, value.length);

    // Provide context for the current selection.
    const context = baseLineOnStart ? cur.startContainer : cur.endContainer;
    if (context && context.role !== RoleType.TEXT_FIELD) {
      const output = new Output().suppress('name').withBraille(
          CursorRange.fromNode(context), CursorRange.fromNode(this.node_),
          OutputCustomEvent.NAVIGATE);
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

    let start = cur.startOffset;
    let end = cur.endOffset;
    if (isFirstLine) {
      if (!/\s/.test(value.toString()[value.length - 1])) {
        value.append(Output.SPACE);
      }

      if (isEmpty) {
        // When the text field is empty, place the selection cursor immediately
        // after the space and before the 'ed' role msg indicator below.
        start = value.length - 1;
        end = start;
      }
      value.append(Msgs.getMsg('tag_textarea_brl'));
    }

    value.setSpan(new ValueSelectionSpan(), start, end);
    ChromeVox.braille.write(
        new NavBraille({text: value, startIndex: start, endIndex: end}));
  }

  /**
   * @param {AutomationNode|undefined} startNode
   * @param {number} startOffset
   * @param {AutomationNode|undefined} endNode
   * @param {number} endOffset
   */
  speakTextSelection_(startNode, startOffset, endNode, endOffset) {
    if (!startNode || !endNode) {
      return;
    }

    const selectedRange = new CursorRange(
        new Cursor(startNode, startOffset), new Cursor(endNode, endOffset));

    new Output()
        .withRichSpeech(
            selectedRange, CursorRange.fromNode(this.node_),
            OutputCustomEvent.NAVIGATE)
        .go();
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
      msgs.forEach(
          msg => ChromeVox.tts.speak(
              Msgs.getMsg(msg), QueueMode.QUEUE, Personality.ANNOTATION));
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
      msgs.forEach(
          msgObject => ChromeVox.tts.speak(
              Msgs.getMsg(msgObject.msg, msgObject.opt_subs), QueueMode.QUEUE,
              Personality.ANNOTATION));
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
    AutomationEditableText.prototype.changed.call(this, evt);
  }

  /**
   * @private
   * @param {EditableLine} cur Current line.
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
   * @param {!EditableLine} cur
   * @param {!EditableLine} prev
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
   * @param {!EditableLine} cur
   * @private
   */
  speakAllMarkers_(cur) {
    const container = cur.startContainer;
    if (!container) {
      return;
    }

    this.speakTextMarker_(container, cur.localStartOffset, cur.localEndOffset);

    if (SettingsManager.get('announceRichTextAttributes')) {
      this.speakTextStyle_(container);
    }
  }
}

/**
 * An observer that reacts to ChromeVox range changes that modifies braille
 * table output when over email or url text fields.
 * @implements {ChromeVoxRangeObserver}
 */
export class EditingRangeObserver {
  constructor() {
    ChromeVoxState.ready().then(() => ChromeVoxRange.addObserver(this));
  }

  static init() {
    if (EditingRangeObserver.instance) {
      throw new Error('Cannot call EditingRangeObserver.init more than once');
    }
    EditingRangeObserver.instance = new EditingRangeObserver();
  }

  /**
   * @param {CursorRange} range
   * @param {boolean=} opt_fromEditing
   * @override
   */
  onCurrentRangeChanged(range, opt_fromEditing) {
    const inputType = range && range.start.node.inputType;
    if (inputType === 'email' || inputType === 'url') {
      BrailleTranslatorManager.instance.refresh(
          SettingsManager.getString('brailleTable8'));
      return;
    }
    BrailleTranslatorManager.instance.refresh(
        SettingsManager.getString('brailleTable'));
  }
}

/** @type {ChromeVoxRangeObserver} */
EditingRangeObserver.instance;
