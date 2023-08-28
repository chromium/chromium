// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '../../../common/automation_predicate.js';
import {AutomationUtil} from '../../../common/automation_util.js';
import {constants} from '../../../common/constants.js';
import {CursorRange} from '../../../common/cursors/range.js';
import {ChromeVoxEvent} from '../../common/custom_automation_event.js';
import {ChromeVoxState} from '../chromevox_state.js';

import {EditableLine} from './editable_line.js';
import {AutomationEditableText, AutomationRichEditableText} from './editing.js';

const AutomationIntent = chrome.automation.AutomationIntent;
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const IntentCommandType = chrome.automation.IntentCommandType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/**
 * A handler for automation events in a focused text field or editable root
 * such as a |contenteditable| subtree.
 */
export class TextEditHandler {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    /** @const {!AutomationNode} @private */
    this.node_ = node;

    if (!node.state[StateType.EDITABLE]) {
      throw new Error('|node| must be editable.');
    }

    /** @private {!AutomationEditableText} */
    this.editableText_ = this.createEditableText_();

    /** @private {!Array<AutomationIntent>} */
    this.inferredIntents_ = [];
  }

  /**
   * ChromeVox handles two general groups of text fields:
   * A rich text field is one where selection gets placed on a DOM
   * descendant to a root text field. This is one of:
   * - content editables (detected via editable state and contenteditable
   * html attribute, or just richly editable state)
   * - text areas (<textarea>) detected via its html tag
   *
   * A non-rich text field is one where accessibility only provides a value,
   * and a pair of numbers for the selection start and end. ChromeVox places
   * single-lined text fields, including those from web content, and ARC++
   * in this group. In addition, multiline ARC++ text fields are treated
   * this way.
   *
   * Note that these definitions slightly differ from those in Blink, which
   * only considers text fields in web content.
   * @return {boolean}
   */
  useRichText_() {
    return this.node_.state[StateType.RICHLY_EDITABLE] ||
        // This condition is a full proof way to ensure the node is editable
        // and has the content editable attribute set to any valid value.
        (this.node_.state[StateType.EDITABLE] && this.node_.htmlAttributes &&
         this.node_.htmlAttributes['contenteditable'] !== undefined &&
         this.node_.htmlAttributes['contenteditable'] !== 'false') ||
        false;
  }

  /**
   * @return {!AutomationEditableText}
   * @private
   */
  createEditableText_() {
    const isTextArea = this.node_.htmlTag === 'textarea';

    const useRichText = this.useRichText_() || isTextArea;

    // Prior to creating the specific editable text handler, ensure that text
    // areas exclude offscreen elements in line computations. This is because
    // text areas from Blink expose a single large static text node which can
    // have thousands or more inline text boxes. This is a very specific check
    // because ignoring offscreen nodes can impact the way in which we convert
    // from a tree position to a deep equivalent on the inline text boxes.
    const firstStaticText = this.node_.find({role: RoleType.STATIC_TEXT});
    EditableLine.includeOffscreen = !isTextArea || !firstStaticText ||
        firstStaticText.children.length < MAX_INLINE_TEXT_BOXES;

    return useRichText ? new AutomationRichEditableText(this.node_) :
                         new AutomationEditableText(this.node_);
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

    let intents = evt.intents;

    // Check for inferred intents applied by other modules e.g. CommandHandler.
    // Be strict about what's allowed and limit only to overriding set
    // selections.
    if (this.inferredIntents_.length > 0 &&
        (intents.length === 0 || intents.some(isSetOrClear))) {
      intents = this.inferredIntents_;
    }
    this.inferredIntents_ = [];

    this.editableText_.onUpdate(intents);
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
        {skipInitialSubtree: true});
    ChromeVoxState.instance.navigateToRange(
        CursorRange.fromNode(after ?? this.node_));
  }

  /**
   * Injects intents into the stream of editing events. In particular, |intents|
   * will be applied to the next processed edfiting event.
   * @param {!Array<AutomationIntent>} intents
   */
  injectInferredIntents(intents) {
    this.inferredIntents_ = intents;
  }

  /**
   * @param {!AutomationNode} node The root editable node, i.e. the root of a
   *     contenteditable subtree or a text field.
   * @return {TextEditHandler}
   */
  static createForNode(node) {
    if (!node.state.editable) {
      throw new Error('Expected editable node.');
    }

    return new TextEditHandler(node);
  }
}

// Local to module.

/** @type {number} */
const MAX_INLINE_TEXT_BOXES = 500;

/**
 * @param {!AutomationIntent} intent
 * @return {boolean}
 */
function isSetOrClear(intent) {
  return intent.command === IntentCommandType.SET_SELECTION ||
      intent.command === IntentCommandType.CLEAR_SELECTION;
}
