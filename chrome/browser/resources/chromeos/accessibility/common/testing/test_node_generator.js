// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParagraphUtils} from '../paragraph_utils.js';

/**
 * Creates a AutomationNode-like object.
 * @param {!Object} properties
 */
export function createMockNode(properties) {
  const node = Object.assign(
      {
        state: {},
        children: [],
        unclippedLocation: {left: 20, top: 10, width: 100, height: 50},
        location: {left: 20, top: 10, width: 100, height: 50},
      },
      properties);

  if (node.parent) {
    // Update children of parent and sibling properties.
    const parent = node.parent;
    if (parent.children.length === 0) {
      parent.children = [];
      parent.firstChild = node;
    } else {
      node.previousSibling = parent.children[parent.children.length - 1];
      node.previousSibling.nextSibling = node;
    }
    parent.children.push(node);
    parent.lastChild = node;
  }
  return node;
}

/**
 * Creates a nodeGroup for test purpose.
 * @return {!ParagraphUtils.NodeGroup}
 */
export function generateTestNodeGroup() {
  const root = createMockNode({role: 'rootWebArea'});
  const paragraph =
      createMockNode({role: 'paragraph', display: 'block', parent: root, root});
  const text1 = createMockNode(
      {name: 'The first sentence.', role: 'staticText', parent: paragraph});
  const inlineText1 = createMockNode({
    role: 'inlineTextBox',
    name: 'The first',
    indexInParent: 0,
    parent: text1,
  });
  const inlineText2 = createMockNode({
    role: 'inlineTextBox',
    name: ' sentence.',
    indexInParent: 1,
    parent: text1,
  });

  const text2 = createMockNode({
    name: 'The second sentence is longer.',
    role: 'staticText',
    parent: paragraph,
  });
  const inlineText3 = createMockNode({
    role: 'inlineTextBox',
    name: 'The second',
    indexInParent: 0,
    parent: text2,
  });
  const inlineText4 = createMockNode({
    role: 'inlineTextBox',
    name: ' sentence is longer.',
    indexInParent: 1,
    parent: text2,
  });

  const text3 = createMockNode(
      {name: 'No child sentence.', role: 'staticText', parent: paragraph});

  return ParagraphUtils.buildNodeGroup(
      [inlineText1, inlineText2, inlineText3, inlineText4, text3], 0,
      false /* do not split on language */);
}
