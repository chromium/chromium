// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {NodeStore} from './node_store.js';

export interface SelectionWithIds {
  anchorNodeId?: number;
  anchorOffset: number;
  focusNodeId?: number;
  focusOffset: number;
}

// Handles the business logic for selection in the Reading mode panel.
export class SelectionController {
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private scrollingOnSelection_: boolean = false;

  getSelectionAdjustedForHighlights(
      anchorNode: Node, anchorOffset: number, focusNode: Node,
      focusOffset: number): SelectionWithIds {
    let anchorNodeId = this.nodeStore_.getAxId(anchorNode);
    let focusNodeId = this.nodeStore_.getAxId(focusNode);
    let adjustedAnchorOffset = anchorOffset;
    let adjustedFocusOffset = focusOffset;
    if (!anchorNodeId) {
      const ancestor = this.nodeStore_.getAncestor(anchorNode);
      if (ancestor) {
        anchorNodeId = this.nodeStore_.getAxId(ancestor.node);
        adjustedAnchorOffset += ancestor.offset;
      }
    }
    if (!focusNodeId) {
      const ancestor = this.nodeStore_.getAncestor(focusNode);
      if (ancestor) {
        focusNodeId = this.nodeStore_.getAxId(ancestor.node);
        adjustedFocusOffset += ancestor.offset;
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
    chrome.readingMode.onScroll(this.scrollingOnSelection_);
    this.scrollingOnSelection_ = false;
  }

  updateSelection(selection: Selection) {
    selection.removeAllRanges();
    const range = new Range();
    const startNodeId = chrome.readingMode.startNodeId;
    const endNodeId = chrome.readingMode.endNodeId;
    let startOffset = chrome.readingMode.startOffset;
    let endOffset = chrome.readingMode.endOffset;
    let startNode = this.nodeStore_.getDomNode(startNodeId);
    let endNode = this.nodeStore_.getDomNode(endNodeId);
    if (!startNode || !endNode) {
      return;
    }

    // Range.setStart/setEnd behaves differently if the node is an element or a
    // text node. If the former, the offset refers to the index of the children.
    // If the latter, the offset refers to the character offset inside the text
    // node. The start and end nodes are elements if they've been read aloud
    // because we add formatting to the text that wasn't there before. However,
    // the information we receive from chrome.readingMode is always the id of a
    // text node and character offset for that text, so find the corresponding
    // text child here and adjust the offset
    if (startNode.nodeType !== Node.TEXT_NODE) {
      const startTreeWalker =
          document.createTreeWalker(startNode, NodeFilter.SHOW_TEXT);
      while (startTreeWalker.nextNode()) {
        const textNodeLength = startTreeWalker.currentNode.textContent!.length;
        // Once we find the child text node inside which the starting index
        // fits, update the start node to be that child node and the adjusted
        // offset will be relative to this child node
        if (startOffset < textNodeLength) {
          startNode = startTreeWalker.currentNode;
          break;
        }

        startOffset -= textNodeLength;
      }
    }
    if (endNode.nodeType !== Node.TEXT_NODE) {
      const endTreeWalker =
          document.createTreeWalker(endNode, NodeFilter.SHOW_TEXT);
      while (endTreeWalker.nextNode()) {
        const textNodeLength = endTreeWalker.currentNode.textContent!.length;
        if (endOffset <= textNodeLength) {
          endNode = endTreeWalker.currentNode;
          break;
        }

        endOffset -= textNodeLength;
      }
    }

    // Gmail will try to select text when collapsing the node. At the same time,
    // the node contents are then shortened because of the collapse which causes
    // the range to go out of bounds. When this happens we should reset the
    // selection.
    try {
      range.setStart(startNode, startOffset);
      range.setEnd(endNode, endOffset);
    } catch (err) {
      selection.removeAllRanges();
      return;
    }

    selection.addRange(range);

    // Scroll the start node into view. ScrollIntoView is available on the
    // Element class.
    const startElement = startNode.nodeType === Node.ELEMENT_NODE ?
        startNode as Element :
        startNode.parentElement;
    if (!startElement) {
      return;
    }
    this.scrollingOnSelection_ = true;
    startElement.scrollIntoViewIfNeeded();
  }


  static getInstance(): SelectionController {
    return instance || (instance = new SelectionController());
  }

  static setInstance(obj: SelectionController) {
    instance = obj;
  }
}

let instance: SelectionController|null = null;
