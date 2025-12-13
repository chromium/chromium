// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {NodeStore} from './node_store.js';

interface SelectionWithIds {
  anchorNodeId?: number;
  anchorOffset: number;
  focusNodeId?: number;
  focusOffset: number;
}

interface ReadOnlySelection {
  anchorNode: Node;
  anchorOffset: number;
  focusNode: Node;
  focusOffset: number;
}

export interface SelectionEndpoint {
  nodeId?: number;
  offset: number;
}

// Handles the business logic for selection in the Reading mode panel.
export class SelectionController {
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private scrollingOnSelection_: boolean = false;
  private currentSelection_: Selection|null = null;
  private isSelectionFromMainPanel_: boolean = false;

  hasSelection(): boolean {
    const selection = this.currentSelection_;
    return (selection !== null) &&
        (selection.anchorNode !== selection.focusNode ||
         selection.anchorOffset !== selection.focusOffset);
  }

  getCurrentSelectionStart(): SelectionEndpoint {
    const anchorNodeId = chrome.readingMode.startNodeId;
    const anchorOffset = chrome.readingMode.startOffset;
    const focusNodeId = chrome.readingMode.endNodeId;
    const focusOffset = chrome.readingMode.endOffset;

    // If only one of the ids is present, use that one.
    let startingNodeId: number|undefined =
        anchorNodeId ? anchorNodeId : focusNodeId;
    let startingOffset = anchorNodeId ? anchorOffset : focusOffset;
    // If both are present, start with the node that is sooner in the page.
    if (anchorNodeId && focusNodeId) {
      const selection = this.currentSelection_;
      if (anchorNodeId === focusNodeId) {
        startingOffset = Math.min(anchorOffset, focusOffset);
      } else if (selection && selection.anchorNode && selection.focusNode) {
        const pos =
            selection.anchorNode.compareDocumentPosition(selection.focusNode);
        const focusIsFirst = pos === Node.DOCUMENT_POSITION_PRECEDING;
        startingNodeId = focusIsFirst ? focusNodeId : anchorNodeId;
        startingOffset = focusIsFirst ? focusOffset : anchorOffset;
      }
    }

    return {nodeId: startingNodeId, offset: startingOffset};
  }

  // Called when the user selects text in reading mode. Forwards that
  // information to the main panel to draw the corresponding selection there.
  onSelectionChange(selection: Selection|null) {
    this.currentSelection_ = selection;

    // No need to send the selection info back to the main panel if it came
    // from there.
    if (this.isSelectionFromMainPanel_) {
      this.isSelectionFromMainPanel_ = false;
      return;
    }

    if ((selection === null) || !selection.anchorNode || !selection.focusNode) {
      // The selection was collapsed by clicking inside the selection.
      chrome.readingMode.onCollapseSelection();
      return;
    }

    const {anchorNodeId, anchorOffset, focusNodeId, focusOffset} =
        this.getSelectionIds_(
            selection.anchorNode, selection.anchorOffset, selection.focusNode,
            selection.focusOffset);
    if (!anchorNodeId || !focusNodeId) {
      return;
    }

    // Only send this selection to the main panel if it is different than the
    // current main panel selection.
    const mainPanelAnchor =
        this.nodeStore_.getDomNode(chrome.readingMode.startNodeId);
    const mainPanelFocus =
        this.nodeStore_.getDomNode(chrome.readingMode.endNodeId);
    if (!mainPanelAnchor || !mainPanelAnchor.contains(selection.anchorNode) ||
        !mainPanelFocus || !mainPanelFocus.contains(selection.focusNode) ||
        selection.anchorOffset !== chrome.readingMode.startOffset ||
        selection.focusOffset !== chrome.readingMode.endOffset) {
      chrome.readingMode.onSelectionChange(
          anchorNodeId, anchorOffset, focusNodeId, focusOffset);
    }
  }

  private getSelectionIds_(
      anchorNode: Node, anchorOffset: number, focusNode: Node,
      focusOffset: number): SelectionWithIds {
    let anchorNodeId = this.nodeStore_.getAxId(anchorNode);
    let focusNodeId = this.nodeStore_.getAxId(focusNode);
    let adjustedAnchorOffset = anchorOffset;
    let adjustedFocusOffset = focusOffset;
    if (chrome.readingMode.isReadAloudEnabled) {
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

  // Updates the reading mode selection to match the main panel selection.
  updateSelection(selectionToUpdate: Selection|null, container: Node) {
    if (!selectionToUpdate) {
      return;
    }
    const {startNodeId, endNodeId} = chrome.readingMode;
    if (!startNodeId || !endNodeId) {
      // The selection is the main panel collapsed, so clear the selection here.
      selectionToUpdate.removeAllRanges();
      return;
    }

    const newSelection = this.getNewSelection_(container);
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

    // Scroll the anchor node into view. ScrollIntoView is available on the
    // Element class.
    const anchorElement =
        newSelection.anchorNode.nodeType === Node.ELEMENT_NODE ?
        newSelection.anchorNode as Element :
        newSelection.anchorNode.parentElement;
    if (!anchorElement) {
      return;
    }
    this.scrollingOnSelection_ = true;
    anchorElement.scrollIntoViewIfNeeded();
  }

  private getNewSelection_(container: Node): ReadOnlySelection|null {
    const selectionIds = {
      anchorNodeId: chrome.readingMode.startNodeId,
      anchorOffset: chrome.readingMode.startOffset,
      focusNodeId: chrome.readingMode.endNodeId,
      focusOffset: chrome.readingMode.endOffset,
    };

    return chrome.readingMode.isReadabilityEnabled ?
        this.getNewSelectionWithoutAxIds_(container, selectionIds) :
        this.getNewSelectionWithAxIds_(selectionIds);
  }

  private getNewSelectionWithoutAxIds_(
      container: Node, selectionIds: SelectionWithIds): ReadOnlySelection|null {
    const {anchorNodeId, focusNodeId} = selectionIds;
    let {anchorOffset, focusOffset} = selectionIds;
    if (!anchorNodeId || !focusNodeId) {
      return null;
    }

    const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
    const anchorContext = {
      prefix: chrome.readingMode.getPrefixText(anchorNodeId),
      content: chrome.readingMode.getTextContent(anchorNodeId),
    };
    const focusContext = {
      prefix: chrome.readingMode.getPrefixText(focusNodeId),
      content: chrome.readingMode.getTextContent(focusNodeId),
    };

    const result = this.findTargetNodes_(walker, anchorContext, focusContext);
    if (!result) {
      return null;
    }

    const anchorNode = result.startNode;
    const focusNode = result.endNode;
    const isSameNode = anchorNodeId === focusNodeId;
    const originalFocus =
        isSameNode ? focusOffset : anchorContext.content.length;
    const originalContent =
        anchorContext.content.substring(anchorOffset, originalFocus);
    const actualContent =
        anchorNode.textContent?.substring(anchorOffset, originalFocus);
    // If the actually selected text matches the found Reading mode text, use
    // the original offsets. Otherwise, search for the selected text within
    // the Reading mode node and use that as the offset.
    if (actualContent !== originalContent) {
      const foundIndex = anchorNode.textContent?.indexOf(originalContent) ?? -1;
      if (foundIndex < 0) {
        return null;
      }

      anchorOffset = foundIndex;
      focusOffset =
          isSameNode ? (foundIndex + originalContent.length) : focusOffset;
    }

    return {anchorNode, anchorOffset, focusNode, focusOffset};
  }

  private getNewSelectionWithAxIds_(selectionIds: SelectionWithIds):
      ReadOnlySelection|null {
    const {anchorNodeId, focusNodeId} = selectionIds;
    let {anchorOffset, focusOffset} = selectionIds;
    if (!anchorNodeId || !focusNodeId) {
      return null;
    }

    let anchorNode = this.nodeStore_.getDomNode(anchorNodeId);
    let focusNode = this.nodeStore_.getDomNode(focusNodeId);
    if (!anchorNode || !focusNode) {
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
    if (anchorNode.nodeType !== Node.TEXT_NODE) {
      const startTreeWalker =
          document.createTreeWalker(anchorNode, NodeFilter.SHOW_TEXT);
      while (startTreeWalker.nextNode()) {
        const textNodeLength = startTreeWalker.currentNode.textContent!.length;
        // Once we find the child text node inside which the starting index
        // fits, update the start node to be that child node and the adjusted
        // offset will be relative to this child node
        if (anchorOffset < textNodeLength) {
          anchorNode = startTreeWalker.currentNode;
          break;
        }

        anchorOffset -= textNodeLength;
      }
    }
    if (focusNode.nodeType !== Node.TEXT_NODE) {
      const endTreeWalker =
          document.createTreeWalker(focusNode, NodeFilter.SHOW_TEXT);
      while (endTreeWalker.nextNode()) {
        const textNodeLength = endTreeWalker.currentNode.textContent!.length;
        if (focusOffset <= textNodeLength) {
          focusNode = endTreeWalker.currentNode;
          break;
        }

        focusOffset -= textNodeLength;
      }
    }

    return {anchorNode, anchorOffset, focusNode, focusOffset};
  }

  // Locates selection nodes based on their text content and prefix text.
  // Assumptions made:
  // - start content is <= end content (i.e. either earlier in the document or
  //   the same content) (see PostProcessSelection in read_anything_app_model.cc
  //   which always sends a forward selection)
  // - start prefix is either a superset of the start content or strictly
  //   earlier in the document (see GetPrefixText in
  //   read_anything_node_utils.cc)
  // - end prefix is either a superset of the end content or strictly
  //   earlier in the document
  // Unknowns:
  // - The end prefix may be anywhere relative to the start node depending on
  //   how the prefix is calculated. It could be before the start node if the
  //   start content is too short to be a reliable prefix. It could be the same
  //   as the start node if its content is long enough. Or it could be later
  //   than the start node if the end node is very far after the start node.
  private findTargetNodes_(
      walker: TreeWalker, startContext: {prefix: string, content: string},
      endContext: {prefix: string, content: string}):
      {startNode: Node, endNode: Node}|null {
    let startNode: Node|undefined;
    let endNode: Node|undefined;
    // If the prefix contains the content (i.e. it is a parent of the target
    // node), then skip looking for a prefix.
    let foundStartPrefix = startContext.prefix.includes(startContext.content);
    let foundEndPrefix = endContext.prefix.includes(endContext.content);
    while (walker.nextNode()) {
      const node = walker.currentNode;
      if (foundStartPrefix && node.nodeValue?.includes(startContext.content)) {
        startNode = node;
      }
      if (foundEndPrefix && node.nodeValue?.includes(endContext.content)) {
        endNode = node;
      }

      // Look for the prefix first in order to reduce the risk of matching the
      // same text earlier in the document.
      if (node.nodeValue?.includes(startContext.prefix)) {
        foundStartPrefix = true;
      }
      if (node.nodeValue?.includes(endContext.prefix)) {
        foundEndPrefix = true;
      }

      if (startNode && endNode) {
        return {startNode, endNode};
      }
    }

    return null;
  }

  static getInstance(): SelectionController {
    return instance || (instance = new SelectionController());
  }

  static setInstance(obj: SelectionController) {
    instance = obj;
  }
}

let instance: SelectionController|null = null;
