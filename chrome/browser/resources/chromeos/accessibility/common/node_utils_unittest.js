// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../select_to_speak/select_to_speak_e2e_test_base.js']);

/**
 * Test fixture for node_utils.js.
 */
SelectToSpeakNodeUtilsUnitTest = class extends SelectToSpeakE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await Promise.all([
      importModule('NodeUtils', '/common/node_utils.js'),
      importModule('ParagraphUtils', '/common/paragraph_utils.js'),
      importModule('WordUtils', '/common/word_utils.js'),
      importModule(
          ['createMockNode', 'generateTestNodeGroup'],
          '/common/testing/test_node_generator.js'),
    ]);
  }
};

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'GetNodeVisibilityState', function() {
      const nodeWithoutRoot1 = {root: null};
      const nodeWithoutRoot2 = {root: null, state: {invisible: true}};
      assertEquals(
          NodeUtils.getNodeState(nodeWithoutRoot1),
          NodeUtils.NodeState.NODE_STATE_INVALID);
      assertEquals(
          NodeUtils.getNodeState(nodeWithoutRoot2),
          NodeUtils.NodeState.NODE_STATE_INVALID);

      const invisibleNode1 = {
        root: {},
        parent: {role: ''},
        state: {invisible: true},
      };
      // Currently nodes aren't actually marked 'invisible', so we need to
      // navigate up their tree.
      const invisibleNode2 = {
        root: {},
        parent: {role: 'window', state: {invisible: true}},
        state: {},
      };
      const invisibleNode3 = {root: {}, parent: invisibleNode2, state: {}};
      const invisibleNode4 = {root: {}, parent: invisibleNode3, state: {}};
      assertEquals(
          NodeUtils.getNodeState(invisibleNode1),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(invisibleNode2),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(invisibleNode3),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);

      const normalNode1 = {
        root: {},
        parent: {role: 'window', state: {}},
        state: {},
      };
      const normalNode2 = {root: {}, parent: {normalNode1}, state: {}};
      assertEquals(
          NodeUtils.getNodeState(normalNode1),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(normalNode2),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'GetNodeVisibilityStateWithRootWebArea',
    function() {
      // Currently nodes aren't actually marked 'invisible', so we need to
      // navigate up their tree.
      const window = {root: {}, role: 'window', state: {invisible: true}};
      const rootNode =
          {root: {}, parent: window, state: {}, role: 'rootWebArea'};
      const container = {root: rootNode, parent: rootNode, state: {}};
      const node = {root: rootNode, parent: container, state: {}};
      assertEquals(
          NodeUtils.getNodeState(window),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(container),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(node),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);

      // Make a fake iframe in this invisible window by adding another
      // RootWebArea. The iframe has no root but is parented to the container
      // above.
      const iframeRoot = {parent: container, state: {}, role: 'rootWebArea'};
      const iframeContainer = {root: iframeRoot, parent: iframeRoot, state: {}};
      const iframeNode = {root: iframeRoot, parent: iframeContainer, state: {}};
      assertEquals(
          NodeUtils.getNodeState(iframeContainer),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(iframeNode),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);

      // Make the window visible and try again.
      window.state = {};
      assertEquals(
          NodeUtils.getNodeState(window),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(container),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(node), NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(iframeContainer),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(iframeNode),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
    });

AX_TEST_F('SelectToSpeakNodeUtilsUnitTest', 'findAllMatching', function() {
  const rect = {left: 0, top: 0, width: 100, height: 100};
  const rootNode = {
    root: {},
    state: {},
    role: 'rootWebArea',
    state: {},
    location: {left: 0, top: 0, width: 600, height: 600},
  };
  const container1 = {
    root: rootNode,
    parent: rootNode,
    role: 'staticText',
    name: 'one two',
    state: {},
    location: {left: 0, top: 0, width: 200, height: 200},
  };
  const container2 = {
    root: rootNode,
    parent: rootNode,
    state: {},
    role: 'genericContainer',
    location: {left: 0, top: 0, width: 200, height: 200},
  };
  const node1 = {
    root: rootNode,
    parent: container1,
    name: 'one',
    role: 'inlineTextBox',
    state: {},
    location: {left: 50, top: 0, width: 50, height: 50},
  };
  const node2 = {
    root: rootNode,
    parent: container1,
    name: 'two',
    role: 'inlineTextBox',
    state: {},
    location: {left: 0, top: 50, width: 50, height: 50},
  };
  const node3 = {
    root: rootNode,
    parent: container1,
    value: 'text',
    role: 'textField',
    state: {},
    location: {left: 0, top: 0, width: 25, height: 25},
  };

  // Set up relationships between nodes.
  rootNode.children = [container1, container2];
  rootNode.firstChild = container1;
  container1.nextSibling = container2;
  container1.children = [node1, node2, node3];
  container1.firstChild = node1;
  node1.nextSibling = node2;
  node2.nextSibling = node3;

  // Should get both children of the first container, without getting
  // the first container itself or the empty container.
  let result = [];
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(3, result.length);
  assertEquals(node1, result[0]);
  assertEquals(node2, result[1]);
  assertEquals(node3, result[2]);

  // If a node doesn't have a name, it should not be included.
  result = [];
  node2.name = undefined;
  node3.value = undefined;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node1, result[0]);

  // Try a rect that only overlaps one of the children.
  result = [];
  node2.name = 'two';
  rect.height = 25;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node1, result[0]);

  // Now just overlap a different child.
  result = [];
  rect.top = 50;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node2, result[0]);

  // Offscreen should cause a node to be skipped.
  result = [];
  node2.state = {offscreen: true};
  assertFalse(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(0, result.length);

  // No location should cause a node to be skipped.
  result = [];
  node2.state = {};
  node2.location = undefined;
  assertFalse(NodeUtils.findAllMatching(rootNode, rect, result));

  // A non staticText container without a name should still have
  // children found if they are valid.
  result = [];
  const node4 = {
    root: rootNode,
    parent: container2,
    name: 'four',
    state: {},
    location: {left: 0, top: 50, width: 50, height: 50},
  };
  container2.firstChild = node4;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node4, result[0]);

  // A non staticText container with a valid name should not be
  // read if its children are read. Children take precidence.
  result = [];
  container2.name = 'container2';
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node4, result[0]);

  // A non staticText container with a valid name which has only
  // children without names should be read instead of its children.
  result = [];
  node4.name = undefined;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(container2, result[0]);
});

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'findAllMatchingWithInputs', function() {
      const rect = {left: 0, top: 0, width: 100, height: 100};
      const rootNode = {
        root: {},
        state: {},
        role: 'rootWebArea',
        location: {left: 0, top: 0, width: 600, height: 600},
      };
      const checkbox = {
        root: rootNode,
        parent: rootNode,
        role: 'checkBox',
        state: {},
        location: {left: 0, top: 0, width: 200, height: 200},
        checked: 'true',
      };
      rootNode.children = [checkbox];
      rootNode.firstChild = checkbox;

      const result = [];
      assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
      assertEquals(1, result.length);
      assertEquals(checkbox, result[0]);
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest',
    'getDeepEquivalentForSelectionDeprecatedNoChildren', function() {
      const node = {name: 'Hello, world', children: []};
      let result = NodeUtils.getDeepEquivalentForSelectionDeprecated(node, 0);
      assertEquals(node, result.node);
      assertEquals(0, result.offset);

      result = NodeUtils.getDeepEquivalentForSelectionDeprecated(node, 6);
      assertEquals(node, result.node);
      assertEquals(6, result.offset);
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest',
    'getDeepEquivalentForSelectionDeprecatedSimpleChildren', function() {
      const child1 =
          {name: 'Hello,', children: [], role: 'inlineTextBox', state: {}};
      const child2 =
          {name: ' world', children: [], role: 'inlineTextBox', state: {}};
      const root = {
        name: 'Hello, world',
        children: [child1, child2],
        role: 'staticText',
        state: {},
      };
      child1.parent = root;
      child2.parent = root;
      let result =
          NodeUtils.getDeepEquivalentForSelectionDeprecated(root, 0, true);
      assertEquals(child1, result.node);
      assertEquals(0, result.offset);

      // Get the last index of the first child
      result =
          NodeUtils.getDeepEquivalentForSelectionDeprecated(root, 5, false);
      assertEquals(child1, result.node);
      assertEquals(5, result.offset);

      // Get the first index of the second child
      result = NodeUtils.getDeepEquivalentForSelectionDeprecated(root, 6, true);
      assertEquals(child2, result.node);
      assertEquals(0, result.offset);

      result = NodeUtils.getDeepEquivalentForSelectionDeprecated(root, 9, true);
      assertEquals(child2, result.node);
      assertEquals(3, result.offset);
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest',
    'getDeepEquivalentForSelectionDeprecatedComplexChildren', function() {
      const child1 =
          {name: 'Hello', children: [], role: 'inlineTextBox', state: {}};
      // Empty name
      const child2 =
          {name: undefined, children: [], role: 'inlineTextBox', state: {}};
      const child3 =
          {name: ',', children: [], role: 'inlineTextBox', state: {}};
      const child4 = {
        name: 'Hello,',
        children: [child1, child2, child3],
        role: 'staticText',
        state: {},
        firstChild: child1,
        lastChild: child3,
      };
      child1.parent = child4;
      child2.parent = child4;
      child3.parent = child4;

      const child5 =
          {name: ' ', children: [], role: 'inlineTextBox', state: {}};
      const child6 =
          {name: 'world', children: [], role: 'inlineTextBox', state: {}};
      const child7 = {
        name: ' world',
        children: [child5, child6],
        role: 'staticText',
        state: {},
        firstChild: child5,
        lastChild: child6,
      };
      child5.parent = child7;
      child6.parent = child7;

      const root = {
        name: undefined,
        children: [child4, child7],
        role: 'genericContainer',
        state: {},
        firstChild: child4,
        lastChild: child7,
      };
      child4.parent = root;
      child7.parent = root;

      let result =
          NodeUtils.getDeepEquivalentForSelectionDeprecated(root, 0, true);
      assertEquals(child1, result.node);
      assertEquals(0, result.offset);

      result = NodeUtils.getDeepEquivalentForSelectionDeprecated(root, 1, true);
      assertEquals(child5, result.node);
      assertEquals(0, result.offset);

      result =
          NodeUtils.getDeepEquivalentForSelectionDeprecated(root, 2, false);
      assertEquals(child6, result.node);
      assertEquals(5, result.offset);

      result =
          NodeUtils.getDeepEquivalentForSelectionDeprecated(child4, 2, true);
      assertEquals(child1, result.node);
      assertEquals(2, result.offset);

      result =
          NodeUtils.getDeepEquivalentForSelectionDeprecated(child4, 5, true);
      assertEquals(child3, result.node);
      assertEquals(0, result.offset);
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'sortSvgNodesByReadingOrder', function() {
      const svgRootNode = {role: 'svgRoot'};
      const gNode1 = {
        role: 'genericContainer',
        parent: svgRootNode,
        unclippedLocation: {left: 300, top: 10, width: 100, height: 50},
      };
      const gNode2 = {
        role: 'genericContainer',
        parent: svgRootNode,
        unclippedLocation: {left: 20, top: 10, width: 100, height: 50},
      };
      const textNode1 = {
        role: 'staticText',
        parent: gNode2,
        unclippedLocation: {left: 50, top: 10, width: 20, height: 50},
        name: 'one',
      };
      const textNode2 = {
        role: 'staticText',
        parent: gNode1,
        unclippedLocation: {left: 300, top: 10, width: 20, height: 50},
        name: 'two',
      };
      const textNode3 = {
        role: 'staticText',
        parent: gNode1,
        unclippedLocation: {left: 350, top: 10, width: 20, height: 50},
        name: 'three',
      };

      const nodes = [textNode3, textNode2, textNode1];
      NodeUtils.sortSvgNodesByReadingOrder(nodes);
      assertEquals(nodes[0].name, 'one');
      assertEquals(nodes[1].name, 'two');
      assertEquals(nodes[2].name, 'three');
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'sortNodesByReadingOrderMultipleSVGs',
    function() {
      const textNode1 = {role: 'staticText', name: 'Text Node 1'};
      const svg1RootNode = {role: 'svgRoot'};
      const svg1Node1 = {
        role: 'staticText',
        parent: svg1RootNode,
        unclippedLocation: {left: 0, top: 10, width: 20, height: 50},
        name: 'SVG 1 Node 1',
      };
      const svg1Node2 = {
        role: 'staticText',
        parent: svg1RootNode,
        unclippedLocation: {left: 50, top: 10, width: 20, height: 50},
        name: 'SVG 1 Node 2',
      };
      const textNode2 = {role: 'staticText', name: 'Text Node 2'};
      const svg2RootNode = {role: 'svgRoot'};
      const svg2Node1 = {
        role: 'staticText',
        parent: svg2RootNode,
        unclippedLocation: {left: 300, top: 10, width: 20, height: 50},
        name: 'SVG 2 Node 1',
      };
      const svg2Node2 = {
        role: 'staticText',
        parent: svg2RootNode,
        unclippedLocation: {left: 350, top: 10, width: 20, height: 50},
        name: 'SVG 2 Node 2',
      };
      const textNode3 = {role: 'staticText', name: 'Text Node 3'};

      const nodes = [
        textNode1,
        svg1Node2,
        svg1Node1,
        textNode2,
        svg2Node2,
        svg2Node1,
        textNode3,
      ];
      NodeUtils.sortSvgNodesByReadingOrder(nodes);

      assertEquals(nodes[0].name, 'Text Node 1');
      assertEquals(nodes[1].name, 'SVG 1 Node 1');
      assertEquals(nodes[2].name, 'SVG 1 Node 2');
      assertEquals(nodes[3].name, 'Text Node 2');
      assertEquals(nodes[4].name, 'SVG 2 Node 1');
      assertEquals(nodes[5].name, 'SVG 2 Node 2');
      assertEquals(nodes[6].name, 'Text Node 3');
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'GetAllNodesInParagraph', function() {
      const root = createMockNode({role: 'rootWebArea'});
      const paragraph1 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode(
          {role: 'staticText', parent: paragraph1, root, name: 'Line 1'});
      const text2 = createMockNode(
          {role: 'staticText', parent: paragraph1, root, name: 'Line 2'});
      const paragraph2 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text3 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 3'});
      const text4 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 4'});
      const text5 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 5'});

      let result = NodeUtils.getAllNodesInParagraph(text1);
      assertEquals(result.length, 2);
      assertEquals(result[0], text1);
      assertEquals(result[1], text2);

      result = NodeUtils.getAllNodesInParagraph(text2);
      assertEquals(result.length, 2);
      assertEquals(result[0], text1);
      assertEquals(result[1], text2);

      result = NodeUtils.getAllNodesInParagraph(text3);
      assertEquals(result.length, 3);
      assertEquals(result[0], text3);
      assertEquals(result[1], text4);
      assertEquals(result[2], text5);

      result = NodeUtils.getAllNodesInParagraph(text4);
      assertEquals(result.length, 3);
      assertEquals(result[0], text3);
      assertEquals(result[1], text4);
      assertEquals(result[2], text5);

      result = NodeUtils.getAllNodesInParagraph(text5);
      assertEquals(result.length, 3);
      assertEquals(result[0], text3);
      assertEquals(result[1], text4);
      assertEquals(result[2], text5);
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'getPositionFromNodeGroup', function() {
      // The nodeGroup has four inline text nodes and one static text node.
      // Their starting indexes are 0, 9, 20, 30, and 51. The first and the
      // second inline text nodes belong to one parent, and the third and the
      // forth inline text nodes belong to another parent.
      const nodeGroup = generateTestNodeGroup();

      let testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 0 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[0].node.children[0]);
      assertEquals(testPosition.offset, 0);

      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 4 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[0].node.children[0]);
      assertEquals(testPosition.offset, 4);

      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 10 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[0].node.children[1]);
      assertEquals(testPosition.offset, 1);

      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 20 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[1].node.children[0]);
      assertEquals(testPosition.offset, 0);

      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 30 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[1].node.children[1]);
      assertEquals(testPosition.offset, 0);

      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 39 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[1].node.children[1]);
      assertEquals(testPosition.offset, 9);

      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 52 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[2].node);
      assertEquals(testPosition.offset, 1);

      // The index is out of the text of the node group, fallback to the end.
      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 100 /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[2].node);
      assertEquals(testPosition.offset, 18);

      // The index is out of the text of the node group, fall back to the start.
      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 100 /* charIndex */, false /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[0].node.children[0]);
      assertEquals(testPosition.offset, 0);

      // The index is undefined, fallback to the end.
      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, undefined /* charIndex */, true /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[2].node);
      assertEquals(testPosition.offset, 18);

      // The index is undefined, fall back to the start.
      testPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, undefined /* charIndex */, false /* fallbackToEnd */);
      assertEquals(testPosition.node, nodeGroup.nodes[0].node.children[0]);
      assertEquals(testPosition.offset, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'getDirectionBetweenPositions',
    function() {
      // The nodeGroup has four inline text nodes and one static text node.
      // Their starting indexes are 0, 9, 20, 30, and 51. The first and the
      // second inline text nodes belong to one parent, and the third and the
      // forth inline text nodes belong to another parent.
      const nodeGroup = generateTestNodeGroup();

      let startPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 0, true /* fallbackToEnd */);
      let endPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 1, true /* fallbackToEnd */);
      assertEquals(
          NodeUtils.getDirectionBetweenPositions(startPosition, endPosition),
          constants.Dir.FORWARD);

      startPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 1, true /* fallbackToEnd */);
      endPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 10, true /* fallbackToEnd */);
      assertEquals(
          NodeUtils.getDirectionBetweenPositions(startPosition, endPosition),
          constants.Dir.FORWARD);

      startPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 20, true /* fallbackToEnd */);
      endPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 6, true /* fallbackToEnd */);
      assertEquals(
          NodeUtils.getDirectionBetweenPositions(startPosition, endPosition),
          constants.Dir.BACKWARD);

      startPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 8, true /* fallbackToEnd */);
      endPosition = NodeUtils.getPositionFromNodeGroup(
          nodeGroup, 8, true /* fallbackToEnd */);
      assertEquals(
          NodeUtils.getDirectionBetweenPositions(startPosition, endPosition),
          constants.Dir.BACKWARD);
    });
