// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps each <li> in an <ol> to its number, so each list is only numbered once
// per traversal.
type ListNumbers = Map<HTMLLIElement, number>;

// Returns all visible text nodes in the subtree, including injected list
// markers (e.g. "1. ").
// TODO (crbug.com/507916429): Store text nodes and return them if already
// processed.
export function getReadingModeTextNodes(root: Node): Node[] {
  const textNodes: Node[] = [];
  const walker = createVisibleTreeWalker(root);
  const listNumbers: ListNumbers = new Map();
  let currentNode;

  while ((currentNode = getNextValidNode(walker, listNumbers))) {
    textNodes.push(currentNode);
  }
  return textNodes;
}



// Gets the next valid node for read aloud, given a root node and a starting
// node.
export function getNextValidNodeFromPosition(
    root: Node, startNode: Node|null = null): Node|null {
  const walker = createVisibleTreeWalker(root);
  if (startNode) {
    walker.currentNode = startNode;
  }
  return getNextValidNode(walker);
}

// Creates a TreeWalker configured to skip hidden elements, consistent with
// Reading Mode's visibility rules.
function createVisibleTreeWalker(root: Node): TreeWalker {
  return document.createTreeWalker(root, NodeFilter.SHOW_ALL, {
    acceptNode: (node) => {
      if (node.nodeType === Node.ELEMENT_NODE) {
        const element = node as HTMLElement;
        // Skip display:none and other invisible elements.
        if (element.style.display === 'none' || !element.checkVisibility()) {
          return NodeFilter.FILTER_REJECT;
        }
      }
      return NodeFilter.FILTER_ACCEPT;
    },
  });
}

// Gets the next valid node for reading mode using the given TreeWalker.
// This includes valid marker elements and text nodes.
function getNextValidNode(
    walker: TreeWalker, listNumbers: ListNumbers = new Map()): Node|null {
  let currentNode;
  while (currentNode = walker.nextNode()) {
    if (currentNode.nodeType === Node.ELEMENT_NODE) {
      const marker =
          addNodeForListElement(currentNode as HTMLElement, listNumbers);
      if (marker) {
        return marker;
      }
    } else if (
        currentNode.nodeType === Node.TEXT_NODE && currentNode.textContent) {
      return currentNode;
    }
  }
  return null;
}

function addNodeForListElement(
    element: HTMLElement, listNumbers: ListNumbers): Node|null {
  // If there is an ordered list, add the numbers as read aloud nodes, since
  // these aren't considered "text" nodes and won't be spoken by read aloud
  // otherwise.
  if (element.tagName === 'LI' && element.parentElement &&
      element.parentElement.tagName === 'OL') {
    // Prefer the marker copied from the source page (e.g. "a."), if any.
    let marker = element.dataset['marker'];
    if (!marker) {
      const number = getLiNumber(element as HTMLLIElement, listNumbers);
      marker = number > -1 ? `${number}.` : undefined;
    }

    if (marker) {
      // Create the text node (e.g., "1. "). A newline is added to the
      // beginning of the node to ensure that it is not accidentally
      // grouped with the previous text node for sentence segmentation.
      return document.createTextNode(`\n${marker} `);
    }
  }
  return null;
}


function getLiNumber(liElement: HTMLLIElement, listNumbers: ListNumbers) {
  const cachedNumber = listNumbers.get(liElement);
  if (cachedNumber !== undefined) {
    return cachedNumber;
  }

  const ol = liElement.closest('ol');
  if (!ol) {
    // Not in an ordered list.
    return -1;
  }

  // Get the list's starting number. Default is 1 unless the start attribute
  // is set by the developer.
  let counter = ol.start || 1;

  // Number all <li> elements in the <ol>
  for (const item of ol.children) {
    if (item.tagName !== 'LI') {
      // Skip non-<li> elements
      continue;
    }

    // If the developer set an explicit 'value' on *this* <li>, honor that.
    // If it's 0, it means the attribute isn't set.
    const li = item as HTMLLIElement;
    if (li.value > 0) {
      counter = li.value;
    }

    listNumbers.set(li, counter);
    counter++;
  }

  // Should not be missing.
  return listNumbers.get(liElement) ?? -1;
}
