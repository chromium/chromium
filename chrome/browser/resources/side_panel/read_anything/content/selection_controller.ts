// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {isBlockTrackingEnabled, isDistilledByReadability} from '../shared/common.js';
import {getNearestTextBoundaryPoint, getTextNodeOffsets, isBackwardSelection} from '../shared/dom_queries.js';

import type {ContentBrowserProxy} from './content_browser_proxy.js';
import {ContentBrowserProxyImpl} from './content_browser_proxy.js';
import {NodeStore} from './node_store.js';
import {ContentPositionSource} from './read_anything_types.js';
import type {ContentPosition, ReadingModeSelection, SelectionWithIds} from './read_anything_types.js';
import {TranslatedSelectionMapper} from './translated_selection_mapper.js';

export interface SelectionEndpoint {
  nodeId?: number;
  offset: number;
}

// Handles the business logic for selection in the Reading mode panel.
export class SelectionController {
  private contentBrowserProxy_: ContentBrowserProxy =
      ContentBrowserProxyImpl.getInstance();
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private translatedSelectionMapper_: TranslatedSelectionMapper =
      new TranslatedSelectionMapper();
  private scrollingOnSelection_: boolean = false;
  private currentSelection_: Selection|null = null;
  private isSelectionFromMainPanel_: boolean = false;

  hasSelection(): boolean {
    const selection = this.currentSelection_;
    return (selection !== null) &&
        (selection.anchorNode !== selection.focusNode ||
         selection.anchorOffset !== selection.focusOffset);
  }

  getCurrentSelectionStart(): ContentPosition|null {
    const selection = this.currentSelection_;
    if (!selection || !selection.anchorNode || !selection.focusNode) {
      return null;
    }

    const isBackward = isBackwardSelection(
        selection.anchorNode, selection.anchorOffset, selection.focusNode,
        selection.focusOffset);
    let node: Node = isBackward ? selection.focusNode : selection.anchorNode;
    let offset = isBackward ? selection.focusOffset : selection.anchorOffset;

    const ancestor = this.nodeStore_.getAncestor(node);
    if (ancestor) {
      node = ancestor.node;
      offset += ancestor.offset;
    }

    return {node, offset, source: ContentPositionSource.SELECTION};
  }

  // Called when the user selects text in reading mode. Forwards that
  // information to the main panel to draw the corresponding selection there.
  onSelectionChange(
      selection: Selection|null, contentContainer: Node = document.body) {
    this.currentSelection_ = selection;

    // No need to send the selection info back to the main panel if it came
    // from there.
    if (this.isSelectionFromMainPanel_) {
      this.isSelectionFromMainPanel_ = false;
      return;
    }

    // Only notify about selection changes if the side panel has focus. This
    // ensures the change originates from direct user interaction within the
    // side panel.
    if (!this.contentBrowserProxy_.hasFocus()) {
      return;
    }

    if ((selection === null) || !selection.anchorNode || !selection.focusNode ||
        selection.isCollapsed) {
      // The selection was collapsed by clicking inside the selection.
      this.contentBrowserProxy_.onCollapseSelection();
      return;
    }

    // Translated text can't be mapped to the main panel's text exactly, so map
    // the selection to the main panel one block at a time instead.
    if (this.shouldUseBlockMode_()) {
      const readingModeSelection: ReadingModeSelection = {
        anchorNode: selection.anchorNode,
        anchorOffset: selection.anchorOffset,
        focusNode: selection.focusNode,
        focusOffset: selection.focusOffset,
      };
      this.sendBlockSelection_(readingModeSelection, contentContainer);
      return;
    }

    // If we're using Readability, report a user selection attempt to try to log
    // this as an early selection attempt from the side panel.
    if (isDistilledByReadability()) {
      this.contentBrowserProxy_.attemptLogEarlySelection(
          /*fromSidePanel=*/ true);
    }

    // Determine the direction of the selection (dragging forward vs backward).
    // This is necessary because normalizeBoundaryPoint_ requires knowing
    // whether an endpoint is the start of the selection (isStart = true) or the
    // end (isStart = false) to perform correct boundary shifting without
    // leaking into preceding elements (like the article title on backward
    // drags).
    const isBackward = isBackwardSelection(
        selection.anchorNode, selection.anchorOffset, selection.focusNode,
        selection.focusOffset);

    // Normalize the selection boundaries to ensure they fall on valid text
    // nodes within the article container, adjusting for the direction of
    // selection to prevent index shifting errors.
    const normalizedAnchor = getNearestTextBoundaryPoint(
        selection.anchorNode, selection.anchorOffset, contentContainer,
        !isBackward, node => this.isValidSelectionTarget_(node));
    const normalizedFocus = getNearestTextBoundaryPoint(
        selection.focusNode, selection.focusOffset, contentContainer,
        isBackward, node => this.isValidSelectionTarget_(node));
    const {anchorNodeId, anchorOffset, focusNodeId, focusOffset} =
        this.getSelectionIds_(
            normalizedAnchor.node, normalizedAnchor.offset,
            normalizedFocus.node, normalizedFocus.offset);


    if (!anchorNodeId || !focusNodeId) {
      // The selection is on a node that doesn't map to the article text (e.g.
      // the background or UI elements). Collapse the main panel selection to
      // match the resulting collapsed state in the side panel.
      this.contentBrowserProxy_.onCollapseSelection();
      return;
    }

    // Only send this selection to the main panel if it is different than the
    // current main panel selection.
    const mainPanelAnchor =
        this.nodeStore_.getDomNode(this.contentBrowserProxy_.getStartNodeId());
    const mainPanelFocus =
        this.nodeStore_.getDomNode(this.contentBrowserProxy_.getEndNodeId());
    if (!mainPanelAnchor || !mainPanelAnchor.contains(normalizedAnchor.node) ||
        !mainPanelFocus || !mainPanelFocus.contains(normalizedFocus.node) ||
        anchorOffset !== this.contentBrowserProxy_.getStartOffset() ||
        focusOffset !== this.contentBrowserProxy_.getEndOffset()) {
      this.contentBrowserProxy_.onSelectionChange(
          anchorNodeId, anchorOffset, focusNodeId, focusOffset);
    }
  }

  private getSelectionIds_(
      anchorNode: Node, anchorOffset: number, focusNode: Node,
      focusOffset: number): SelectionWithIds {
    let anchorNodeId = this.nodeStore_.getAxId(anchorNode);
    let focusNodeId = this.nodeStore_.getAxId(focusNode);
    let adjustedAnchorOffset =
        anchorOffset + this.nodeStore_.getAxNodeOffset(anchorNode);
    let adjustedFocusOffset =
        focusOffset + this.nodeStore_.getAxNodeOffset(focusNode);
    if (!anchorNodeId) {
      const ancestor = this.nodeStore_.getAncestor(anchorNode);
      if (ancestor) {
        anchorNodeId = this.nodeStore_.getAxId(ancestor.node);
        adjustedAnchorOffset +=
            ancestor.offset + this.nodeStore_.getAxNodeOffset(ancestor.node);
      }
    }
    if (!focusNodeId) {
      const ancestor = this.nodeStore_.getAncestor(focusNode);
      if (ancestor) {
        focusNodeId = this.nodeStore_.getAxId(ancestor.node);
        adjustedFocusOffset +=
            ancestor.offset + this.nodeStore_.getAxNodeOffset(ancestor.node);
      }
    }
    return {
      anchorNodeId: anchorNodeId,
      anchorOffset: adjustedAnchorOffset,
      focusNodeId: focusNodeId,
      focusOffset: adjustedFocusOffset,
    };
  }

  onScroll() {
    this.contentBrowserProxy_.onScroll(this.scrollingOnSelection_);
    this.scrollingOnSelection_ = false;
  }

  // Updates the reading mode selection to match the main panel selection.
  updateSelection(
      selectionToUpdate: Selection|null,
      contentContainer: Node = document.body) {
    if (!selectionToUpdate) {
      return;
    }

    const startNodeId = this.contentBrowserProxy_.getStartNodeId();
    const endNodeId = this.contentBrowserProxy_.getEndNodeId();
    const hasValidSelection = this.contentBrowserProxy_.hasValidSelection();
    if (!startNodeId || !endNodeId || !hasValidSelection) {
      // The selection in the main panel collapsed, so clear the selection here.
      selectionToUpdate.removeAllRanges();
      return;
    }

    // If we're using Readability, report a user selection attempt to try to log
    // this as an early selection attempt from the main panel.
    if (isDistilledByReadability()) {
      this.contentBrowserProxy_.attemptLogEarlySelection(
          /*fromSidePanel=*/ false);
    }

    if (this.shouldUseBlockMode_()) {
      this.drawBlockSelection_(selectionToUpdate, contentContainer);
      return;
    }

    const newSelection = this.getNewSelection_();
    if (!newSelection) {
      return;
    }

    // Gmail will try to select text when collapsing the node. At the same time,
    // the node contents are then shortened because of the collapse which causes
    // the range to go out of bounds. When this happens we should reset the
    // selection.
    selectionToUpdate.removeAllRanges();
    const range = new Range();
    try {
      range.setStart(newSelection.anchorNode, newSelection.anchorOffset);
      range.setEnd(newSelection.focusNode, newSelection.focusOffset);
    } catch (err) {
      selectionToUpdate.removeAllRanges();
      return;
    }

    this.isSelectionFromMainPanel_ = true;
    selectionToUpdate.addRange(range);
    this.scrollIntoView_(newSelection.anchorNode);
  }

  // Called when the reading mode content is translated or restored to its
  // original language. Redraws the main panel's selection here, so that it's
  // drawn one block at a time if and only if the content is translated.
  onTranslationStateChanged(selection: Selection|null, contentContainer: Node) {
    if (!this.isBlockModeSupported_()) {
      return;
    }
    this.updateSelection(selection, contentContainer);
  }

  // Whether selections can be synced with the main panel one block at a time.
  // Blocks are only tracked for PDFs, whose reading mode content is translated
  // in the reading mode panel while the main panel shows the original.
  private isBlockModeSupported_(): boolean {
    return isBlockTrackingEnabled() && !isDistilledByReadability();
  }

  // Whether to sync selections with the main panel one block at a time. This
  // is the case if and only if the PDF is translated. It depends only on the
  // translation state reported by the browser, and not on the reading mode
  // panel's DOM, which the translate script rewrites while it translates.
  private shouldUseBlockMode_(): boolean {
    return this.isBlockModeSupported_() &&
        this.contentBrowserProxy_.isContentTranslated();
  }

  // Selects all of the text in the main panel that's in the blocks containing
  // the given reading mode selection.
  private sendBlockSelection_(
      selection: ReadingModeSelection, contentContainer: Node) {
    const axSelection = this.translatedSelectionMapper_.getAxSelection(
        selection, contentContainer);
    if (!axSelection) {
      // No text in a block is selected, e.g. only whitespace or images are
      // selected, so treat this like a selection that doesn't map to the
      // main panel's text.
      this.contentBrowserProxy_.onCollapseSelection();
      return;
    }

    // The same blocks are sent on every selection change while the user
    // drags within them. ReadAnythingAppController::OnSelectionChange() drops
    // the repeats.
    if (this.isMainPanelSelection_(axSelection)) {
      return;
    }

    // The browser ignores selection changes without transient user
    // activation, e.g. those caused by translation rewriting the selected
    // text. Don't send those, since a send that's never echoed back by the
    // main panel stops the next main panel selection from being drawn here.
    if (!this.contentBrowserProxy_.hasTransientUserActivation()) {
      return;
    }

    const {anchorNodeId, anchorOffset, focusNodeId, focusOffset} = axSelection;
    if (anchorNodeId === undefined || focusNodeId === undefined) {
      return;
    }
    this.contentBrowserProxy_.onSelectionChange(
        anchorNodeId, anchorOffset, focusNodeId, focusOffset);
  }

  // Selects the blocks here that contain the main panel selection.
  private drawBlockSelection_(selection: Selection, contentContainer: Node) {
    const blockSelection =
        this.translatedSelectionMapper_.getReadingModeSelection(
            {
              anchorNodeId: this.contentBrowserProxy_.getStartNodeId(),
              anchorOffset: this.contentBrowserProxy_.getStartOffset(),
              focusNodeId: this.contentBrowserProxy_.getEndNodeId(),
              focusOffset: this.contentBrowserProxy_.getEndOffset(),
            },
            contentContainer);
    if (!blockSelection) {
      selection.removeAllRanges();
      return;
    }
    // The selection is forward, so its anchor is the start of the range.
    const range = new Range();
    range.setStart(blockSelection.anchorNode, blockSelection.anchorOffset);
    range.setEnd(blockSelection.focusNode, blockSelection.focusOffset);

    // Don't reselect the same blocks, e.g. when the main panel selection
    // changes within the same blocks. This avoids flicker, and setting
    // isSelectionFromMainPanel_ for a selection change that never happens.
    if (this.isSelectingSameBlocks_(selection, range, contentContainer)) {
      return;
    }

    selection.removeAllRanges();
    this.isSelectionFromMainPanel_ = true;
    selection.addRange(range);
    // The first block is the anchor's child at the anchor offset.
    const startBlock =
        blockSelection.anchorNode.childNodes[blockSelection.anchorOffset];
    if (startBlock) {
      this.scrollIntoView_(startBlock);
    }
  }

  // Whether the selection already selects exactly the whole blocks in the given
  // range. The endpoints can't be compared directly, since the reading mode
  // selection comes from the app's shadow root, which reports canonicalized
  // endpoints, e.g. in the blocks' text rather than in their parent. So this
  // checks that the selection is in the same blocks and selects the same text.
  private isSelectingSameBlocks_(
      selection: Selection, blocksRange: Range,
      contentContainer: Node): boolean {
    if (selection.rangeCount !== 1) {
      return false;
    }
    const current = selection.getRangeAt(0);
    if (current.toString() !== blocksRange.toString()) {
      return false;
    }
    const currentBlocks = this.translatedSelectionMapper_.getAxSelection(
        toReadingModeSelection(current), contentContainer);
    const newBlocks = this.translatedSelectionMapper_.getAxSelection(
        toReadingModeSelection(blocksRange), contentContainer);
    return !!currentBlocks && !!newBlocks &&
        currentBlocks.anchorNodeId === newBlocks.anchorNodeId &&
        currentBlocks.anchorOffset === newBlocks.anchorOffset &&
        currentBlocks.focusNodeId === newBlocks.focusNodeId &&
        currentBlocks.focusOffset === newBlocks.focusOffset;
  }

  // Whether the given selection is the main panel's current selection.
  private isMainPanelSelection_(axSelection: SelectionWithIds): boolean {
    return this.contentBrowserProxy_.hasValidSelection() &&
        axSelection.anchorNodeId ===
        this.contentBrowserProxy_.getStartNodeId() &&
        axSelection.anchorOffset ===
        this.contentBrowserProxy_.getStartOffset() &&
        axSelection.focusNodeId === this.contentBrowserProxy_.getEndNodeId() &&
        axSelection.focusOffset === this.contentBrowserProxy_.getEndOffset();
  }

  // Scrolls the given node into view if needed, after the next frame.
  private scrollIntoView_(node: Node) {
    // ScrollIntoView is available on the Element class.
    const element = node instanceof Element ? node : node.parentElement;
    if (!element) {
      return;
    }
    this.scrollingOnSelection_ = true;
    requestAnimationFrame(() => {
      element.scrollIntoViewIfNeeded();
    });
  }

  private getNewSelection_(): ReadingModeSelection|null {
    const anchorNodeId = this.contentBrowserProxy_.getStartNodeId();
    const focusNodeId = this.contentBrowserProxy_.getEndNodeId();
    let anchorOffset = this.contentBrowserProxy_.getStartOffset();
    let focusOffset = this.contentBrowserProxy_.getEndOffset();
    if (!anchorNodeId || !focusNodeId) {
      return null;
    }

    let anchorNode = this.getDomNodeForAxOffset_(anchorNodeId, anchorOffset);
    let focusNode = this.getDomNodeForAxOffset_(focusNodeId, focusOffset);
    if (!anchorNode || !focusNode) {
      return null;
    }

    // Convert AXNode-relative offsets to offsets relative to the side panel's
    // distilled text fragment.
    anchorOffset -= this.nodeStore_.getAxNodeOffset(anchorNode);
    focusOffset -= this.nodeStore_.getAxNodeOffset(focusNode);

    // Filter out main panel selections that don't overlap the text shown
    // in the side panel.
    if (anchorOffset < 0 ||
        anchorOffset > (anchorNode.textContent?.length ?? 0)) {
      return null;
    }

    // Range.setStart/setEnd behaves differently if the node is an element or
    // a text node. If the former, the offset refers to the index of the
    // children. If the latter, the offset refers to the character offset
    // inside the text node. The start and end nodes are elements if they've
    // been read aloud because we add formatting to the text that wasn't there
    // before. However, the information we receive from chrome.readingMode is
    // always the id of a text node and character offset for that text, so
    // find the corresponding text child here and adjust the offset
    const adjustedAnchor = getTextNodeOffsets(anchorNode, anchorOffset);
    const adjustedFocus = getTextNodeOffsets(focusNode, focusOffset);
    anchorNode = adjustedAnchor.node;
    anchorOffset -= adjustedAnchor.offset;
    focusNode = adjustedFocus.node;
    focusOffset -= adjustedFocus.offset;

    return {anchorNode, anchorOffset, focusNode, focusOffset};
  }

  // With Readability, one AXNode's text can be split across several sibling
  // DOM nodes (see ContentController.mapBlockToAxNodes_), but the NodeStore
  // only returns one of them for an AX ID. Returns the sibling that contains
  // `axOffset`, i.e. the last one whose AX node offset is at or before it.
  // TODO(crbug.com/582908703): Investigate if selection offsets should be
  // applied more broadly.
  private getDomNodeForAxOffset_(axNodeId: number, axOffset: number): Node
      |undefined {
    let result = this.nodeStore_.getDomNode(axNodeId);
    const siblings = result?.parentNode?.childNodes ?? [];
    for (const sibling of siblings) {
      if (this.nodeStore_.getAxId(sibling) === axNodeId &&
          this.nodeStore_.getAxNodeOffset(sibling) <= axOffset) {
        result = sibling;
      }
    }
    return result;
  }

  // Returns true if the node maps to a valid AX ID in the NodeStore.
  private isValidSelectionTarget_(node: Node): boolean {
    return this.nodeStore_.getAxId(node) !== undefined;
  }

  static getInstance(): SelectionController {
    return instance || (instance = new SelectionController());
  }

  static setInstance(obj: SelectionController) {
    instance = obj;
  }
}

// Returns the forward selection with the same endpoints as the range.
function toReadingModeSelection(range: Range): ReadingModeSelection {
  return {
    anchorNode: range.startContainer,
    anchorOffset: range.startOffset,
    focusNode: range.endContainer,
    focusOffset: range.endOffset,
  };
}

let instance: SelectionController|null = null;
