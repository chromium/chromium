// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {constants} from '/common/constants.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ChromeVoxEvent} from '../../common/custom_automation_event.js';
import {ChromeVoxRange} from '../chromevox_range.js';

import {EditableLine} from './editable_line.js';
import {AutomationEditableText} from './editable_text.js';
import {RichEditableText} from './rich_editable_text.js';

import AutomationIntent = chrome.automation.AutomationIntent;
type AutomationNode = chrome.automation.AutomationNode;
import Dir = constants.Dir;
import IntentCommandType = chrome.automation.IntentCommandType;
import RoleType = chrome.automation.RoleType;
import StateType = chrome.automation.StateType;

/**
 * A handler for automation events in a focused text field or editable root
 * such as a |contenteditable| subtree.
 */
export class TextEditHandler {
  readonly node: AutomationNode;
  private editableText_: AutomationEditableText;
  private inferredIntents_: AutomationIntent[] = [];

  constructor(node: AutomationNode) {
    this.node = node;

    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if (!node.state![StateType.EDITABLE]) {
      throw new Error('|node| must be editable.');
    }
    this.editableText_ = this.createEditableText_();
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
   */
  private useRichText_(): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return this.node.state![StateType.RICHLY_EDITABLE] ||
        this.node.nonAtomicTextFieldRoot;
  }

  private createEditableText_(): AutomationEditableText {
    const isTextArea = this.node.htmlTag === 'textarea';

    const useRichText = this.useRichText_() || isTextArea;

    // Prior to creating the specific editable text handler, ensure that text
    // areas exclude offscreen elements in line computations. This is because
    // text areas from Blink expose a single large static text node which can
    // have thousands or more inline text boxes. This is a very specific check
    // because ignoring offscreen nodes can impact the way in which we convert
    // from a tree position to a deep equivalent on the inline text boxes.
    const firstStaticText = this.node.find({role: RoleType.STATIC_TEXT});
    EditableLine.includeOffscreen = !isTextArea || !firstStaticText ||
        firstStaticText.children.length < MAX_INLINE_TEXT_BOXES;

    return useRichText ? new RichEditableText(this.node) :
                         new AutomationEditableText(this.node);
  }

  /**
   * Receives the following kinds of events when the node provided to the
   * constructor is focused: |focus|, |textChanged|, |textSelectionChanged| and
   * |valueInTextFieldChanged|.
   * An implementation of this method should emit the appropriate braille and
   * spoken feedback for the event.
   */
  onEvent(evt: ChromeVoxEvent): void {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if (!evt.target.state!['focused'] || evt.target !== this.node) {
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

  /** Returns true if selection starts at the first line. */
  isSelectionOnFirstLine(): boolean {
    return this.editableText_.isSelectionOnFirstLine();
  }

  /** Returns true if selection ends at the last line. */
  isSelectionOnLastLine(): boolean {
    return this.editableText_.isSelectionOnLastLine();
  }

  /** Moves range to after this text field. */
  moveToAfterEditText(): void {
    const after = AutomationUtil.findNextNode(
        this.node, Dir.FORWARD, AutomationPredicate.object,
        {skipInitialSubtree: true});
    ChromeVoxRange.navigateTo(CursorRange.fromNode(after ?? this.node));
  }

  /**
   * Injects intents into the stream of editing events. In particular, |intents|
   * will be applied to the next processed edfiting event.
   */
  injectInferredIntents(intents: AutomationIntent[]): void {
    this.inferredIntents_ = intents;
  }

  /**
   * @param node The root editable node, i.e. the root of a
   *     contenteditable subtree or a text field.
   */
  static createForNode(node: AutomationNode): TextEditHandler {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if (!node.state!['editable']) {
      throw new Error('Expected editable node.');
    }
    return new TextEditHandler(node);
  }
}

// Local to module.

const MAX_INLINE_TEXT_BOXES = 500;

function isSetOrClear(intent: AutomationIntent): boolean {
  return intent.command === IntentCommandType.SET_SELECTION ||
      intent.command === IntentCommandType.CLEAR_SELECTION;
}

TestImportManager.exportForTesting(TextEditHandler);
