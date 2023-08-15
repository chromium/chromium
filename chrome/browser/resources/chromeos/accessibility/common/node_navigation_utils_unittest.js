// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../select_to_speak/select_to_speak_e2e_test_base.js']);

/**
 * Test fixture for navigation_utils.js.
 */
SelectToSpeakNodeNavigationUtilsUnitTest = class extends SelectToSpeakE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await Promise.all([
      importModule('NodeNavigationUtils', '/common/node_navigation_utils.js'),
      importModule('ParagraphUtils', '/common/paragraph_utils.js'),
      importModule(
          ['createMockNode', 'generateTestNodeGroup'],
          '/common/testing/test_node_generator.js'),
    ]);
  }
};

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest', 'GetNodesForNextParagraph',
    function() {
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

      const nodeGroupForParagraph1 = ParagraphUtils.buildNodeGroup(
          [text1, text2], 0 /* index */, {splitOnLanguage: false});
      const nodeGroupForParagraph2 = ParagraphUtils.buildNodeGroup(
          [text3], 0 /* index */, {splitOnLanguage: false});

      // Navigating forward from paragraph 1 returns the nodes of paragraph 2.
      let result = NodeNavigationUtils.getNodesForNextParagraph(
          nodeGroupForParagraph1, constants.Dir.FORWARD,
          () => true /* does not filter any paragraph */);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      // Navigating backward from paragraph 1 returns no nodes.
      result = NodeNavigationUtils.getNodesForNextParagraph(
          nodeGroupForParagraph1, constants.Dir.BACKWARD,
          () => true /* does not filter any paragraph */);
      assertEquals(result.length, 0);

      // Navigating backward from paragraph 2 returns the nodes of paragraph 1.
      result = NodeNavigationUtils.getNodesForNextParagraph(
          nodeGroupForParagraph2, constants.Dir.BACKWARD,
          () => true /* does not filter any paragraph */);
      assertEquals(result.length, 2);
      assertEquals(result[0], text1);
      assertEquals(result[1], text2);

      // Navigating forward from paragraph 2 returns no nodes.
      result = NodeNavigationUtils.getNodesForNextParagraph(
          nodeGroupForParagraph2, constants.Dir.FORWARD,
          () => true /* does not filter any paragraph */);
      assertEquals(result.length, 0);

      // Navigates forward from paragraph 1 with a pred that filters out
      // paragraph 2.
      result = NodeNavigationUtils.getNodesForNextParagraph(
          nodeGroupForParagraph1, constants.Dir.FORWARD,
          nodes => !(nodes.find(
              n => n.parent ===
                  paragraph2) /* filter out nodes belong to paragraph 2 */));
      assertEquals(result.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest', 'GetNextParagraphWithNode',
    function() {
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

      // Forward from first text node of paragraph 1 gives paragraph 2 nodes.
      let result = NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      // Forward from second text node of paragraph 1 gives paragraph 2 nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      // Forward from paragraph 1 node gives paragraph 2 nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          paragraph1, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      // Forward from text node of paragraph 2 returns no nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text3, constants.Dir.FORWARD);
      assertEquals(result.length, 0);

      // Forward from paragraph 2 returns no nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          paragraph2, constants.Dir.FORWARD);
      assertEquals(result.length, 0);

      // Backward from text node of paragraph 2 returns paragraph 1 nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text3, constants.Dir.BACKWARD);
      assertEquals(result.length, 2);
      assertEquals(result[0], text1);
      assertEquals(result[1], text2);

      // Backward from paragraph 2 node returns paragraph 1 nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          paragraph2, constants.Dir.BACKWARD);
      assertEquals(result.length, 2);
      assertEquals(result[0], text1);
      assertEquals(result[1], text2);

      // Backward from text node of paragraph 1 returns no nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);

      // Backward from text node of paragraph 1 returns no nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);

      // Backward from paragraph 1 node returns no nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          paragraph1, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextParagraphWithNodeContainedWithinRoot', function() {
      const desktop = createMockNode({role: 'desktop'});

      const root = createMockNode({role: 'rootWebArea', parent: desktop});
      const paragraph1 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode(
          {role: 'staticText', parent: paragraph1, root, name: 'Line 1'});
      const otherRoot = createMockNode({role: 'rootWebArea', parent: desktop});
      const paragraph2 = createMockNode(
          {role: 'paragraph', display: 'block', parent: otherRoot, root});
      createMockNode({
        role: 'staticText',
        parent: paragraph2,
        root: otherRoot,
        name: 'Line 2',
      });

      let result = NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.FORWARD);
      assertEquals(result.length, 0);

      NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          paragraph1, constants.Dir.FORWARD);
      assertEquals(result.length, 0);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          paragraph1, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextParagraphWithNodeThroughIframe', function() {
      const desktop = createMockNode({role: 'desktop'});

      const root =
          createMockNode({role: 'rootWebArea', parent: desktop, root: desktop});
      const paragraph1 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode(
          {role: 'staticText', parent: paragraph1, root, name: 'Line 1'});
      const iframeRoot =
          createMockNode({role: 'rootWebArea', parent: root, root});
      const paragraph2 = createMockNode({
        role: 'paragraph',
        display: 'block',
        parent: iframeRoot,
        root: iframeRoot,
      });
      const text2 = createMockNode({
        role: 'staticText',
        parent: paragraph2,
        root: iframeRoot,
        name: 'Line 2',
      });
      const paragraph3 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text3 = createMockNode(
          {role: 'staticText', parent: paragraph3, root, name: 'Line 3'});

      let result = NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text2);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextParagraphWithNodeNonBlockNodes', function() {
      /**
       * Example below is roughly similar to:
       * <p>Line 1</p>
       * <span>Line 2</span>
       * <p><span>Line 2</span><span>Line 3</span></p>
       */
      const root = createMockNode({role: 'rootWebArea'});
      const paragraph1 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode(
          {role: 'staticText', parent: paragraph1, root, name: 'Line 1'});
      const text2 = createMockNode(
          {role: 'staticText', parent: root, root, name: 'Line 2'});
      const paragraph2 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text3 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 3'});
      const text4 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 4'});

      // Forward from first text node of paragraph 1 gives inline text node.
      let result = NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text2);

      // Forward from inline text node gives paragraph 2 nodes.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.FORWARD);
      assertEquals(result.length, 2);
      assertEquals(result[0], text3);
      assertEquals(result[1], text4);

      // Backwards from paragraph 2 inline text node.
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          paragraph2, constants.Dir.BACKWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text2);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextParagraphWithNodeNestedBlocks', function() {
      const root = createMockNode({role: 'rootWebArea'});
      const paragraph1 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode(
          {role: 'staticText', parent: paragraph1, root, name: 'Before text'});
      const nestedParagraph = createMockNode(
          {role: 'paragraph', display: 'block', parent: paragraph1, root});
      const text2 = createMockNode({
        role: 'staticText',
        parent: nestedParagraph,
        root,
        name: 'Middle text',
      });
      const text3 = createMockNode(
          {role: 'staticText', parent: paragraph1, root, name: 'After text'});

      // Getting next paragraph from nested paragraph only includes nodes in
      // the forward direction (does not include itself)
      let result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      // Getting next paragraph from nested paragraph only includes nodes in
      // the backward direction (does not include itself)
      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.BACKWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text1);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextParagraphWithNodeAndroid', function() {
      const root = createMockNode({role: 'application'});
      const container1 =
          createMockNode({role: 'genericContainer', parent: root, root});
      const text1 = createMockNode(
          {role: 'staticText', parent: container1, root, name: 'Line 1'});
      const text2 = createMockNode(
          {role: 'staticText', parent: container1, root, name: 'Line 2'});
      const container2 =
          createMockNode({role: 'genericContainer', parent: root, root});
      const text3 = createMockNode(
          {role: 'staticText', parent: container2, root, name: 'Line 3'});

      // Without paragraphs, navigate node to node.
      let result = NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text2);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          container1, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text3, constants.Dir.FORWARD);
      assertEquals(result.length, 0);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          container2, constants.Dir.FORWARD);
      assertEquals(result.length, 0);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text3, constants.Dir.BACKWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text2);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          container2, constants.Dir.BACKWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text2);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text2, constants.Dir.BACKWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text1);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          text1, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);

      result = NodeNavigationUtils.getNextParagraphWithNode_(
          container1, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextNodesInParagraphFromNodeGroupEmptyNodeGroup', function() {
      const nodeGroup = {nodes: []};
      const result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 0 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 0);
      assertEquals(result.offset, -1);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextNodesInParagraphFromNodeGroupForward', function() {
      // The nodeGroup has four inline text nodes and one static text node.
      // Their starting indexes are 0, 9, 20, 30, and 51.
      const nodeGroup = generateTestNodeGroup();

      let result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 0 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 5);
      assertEquals(result.offset, 0);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 5 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 5);
      assertEquals(result.offset, 5);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 9 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 4);
      assertEquals(result.offset, 0);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 25 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 3);
      assertEquals(result.offset, 5);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 51 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 1);
      assertEquals(result.offset, 0);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 100 /* charIndex */,
          constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextNodesInParagraphFromNodeGroupBackward', function() {
      // The nodeGroup has four inline text nodes and one static text node.
      // Their starting indexes are 0, 9, 20, 30, and 51.
      const nodeGroup = generateTestNodeGroup();

      let result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 0 /* charIndex */, constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 0);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 5 /* charIndex */, constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 1);
      assertEquals(result.offset, 5);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 9 /* charIndex */, constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 1);
      assertEquals(result.offset, 9);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 25 /* charIndex */,
          constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 3);
      assertEquals(result.offset, 5);

      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 51 /* charIndex */,
          constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 4);
      assertEquals(result.offset, 20);

      // The charIndex is out of the range of nodeGroup. It will try to get all
      // the content before the nodeGroup. In this case, there is nothing.
      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 100 /* charIndex */,
          constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextNodesInParagraphFromNodeGroupForwardWithEmptyTail', function() {
      // The nodeGroup consists of three inline text nodes: "Hello", "world ",
      // and " ".
      const nodeGroup = generateTestNodeGroupWithEmptyTail();

      // We can find the non-empty node at this charIndex but there is actually
      // no text content afterwards.
      const nodeWithOffset = ParagraphUtils.findNodeFromNodeGroupByCharIndex(
          nodeGroup, 11 /* charIndex */);
      assertEquals(nodeWithOffset.node.name, 'world ');
      assertEquals(nodeWithOffset.offset, 5);

      let result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 11 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 0);

      // If we decrease the charIndex, we will get the node with 'world ' but
      // not any empty node.
      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 10 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 1);
      assertEquals(result.nodes[0].name, 'world ');
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextNodesInParagraphFromNodeGroupBackwardWithEmptyHeads', function() {
      // The nodeGroup consists of three inline text nodes: " ", " Hello",
      // "world".
      const nodeGroup = generateTestNodeGroupWithEmptyHead();

      // We can find the non-empty node at this charIndex but there is actually
      // no text content before this position.
      const nodeWithOffset = ParagraphUtils.findNodeFromNodeGroupByCharIndex(
          nodeGroup, 2 /* charIndex */);
      assertEquals(nodeWithOffset.node.name, ' Hello');
      assertEquals(nodeWithOffset.offset, 1);

      let result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 2 /* charIndex */, constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 0);

      // If we increase the charIndex, we will get the node with ' Hello' but
      // not any empty node.
      result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 3 /* charIndex */, constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 1);
      assertEquals(result.nodes[0].name, ' Hello');
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextNodesInParagraphFromNodeGroupForwardFromPartialParagraph',
    function() {
      // The nodeGroup consists only one static text node, which is "one". The
      // entire paragraph has three static text nodes: "Sentence", "one",
      // "here".
      const nodeGroup = generateTestNodeGroupFromPartialParagraph();

      // After reading "one", the TTS events will set charIndex to 3. Finding
      // the static text node from the node group will return null.
      const nodeWithOffset = ParagraphUtils.findNodeFromNodeGroupByCharIndex(
          nodeGroup, 3 /* charIndex */);
      assertEquals(nodeWithOffset.node, null);

      const result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 3 /* charIndex */, constants.Dir.FORWARD /* direction */);
      assertEquals(result.nodes.length, 1);
      assertEquals(result.offset, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNextNodesInParagraphFromNodeGroupBackwardFromPartialParagraph',
    function() {
      // The nodeGroup consists only one static text node, which is "one". The
      // entire paragraph has three static text nodes: "Sentence", "one",
      // "here".
      const nodeGroup = generateTestNodeGroupFromPartialParagraph();

      // After reading "one", the TTS events will set charIndex to 3. Finding
      // the static text node from the node group will return null.
      const nodeWithOffset = ParagraphUtils.findNodeFromNodeGroupByCharIndex(
          nodeGroup, 3 /* charIndex */);
      assertEquals(nodeWithOffset.node, null);

      const result = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
          nodeGroup, 3 /* charIndex */, constants.Dir.BACKWARD /* direction */);
      assertEquals(result.nodes.length, 1);
      assertEquals(result.nodes[0].name, 'Sentence');
      assertEquals(result.offset, 8);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest', 'GetNextNodesInParagraph',
    function() {
      const root = createMockNode({role: 'rootWebArea'});
      createMockNode({role: 'paragraph', display: 'block', parent: root, root});
      const paragraph2 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 1'});
      const text2 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 2'});
      const text3 = createMockNode(
          {role: 'staticText', parent: paragraph2, root, name: 'Line 3'});
      createMockNode({role: 'paragraph', display: 'block', parent: root, root});

      let result = NodeNavigationUtils.getNextNodesInParagraph_(
          text2, constants.Dir.FORWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text3);

      result = NodeNavigationUtils.getNextNodesInParagraph_(
          text1, constants.Dir.FORWARD);
      assertEquals(result.length, 2);
      assertEquals(result[0], text2);
      assertEquals(result[1], text3);

      result = NodeNavigationUtils.getNextNodesInParagraph_(
          text3, constants.Dir.FORWARD);
      assertEquals(result.length, 0);

      result = NodeNavigationUtils.getNextNodesInParagraph_(
          text3, constants.Dir.BACKWARD);
      assertEquals(result.length, 2);
      assertEquals(result[0], text1);
      assertEquals(result[1], text2);

      result = NodeNavigationUtils.getNextNodesInParagraph_(
          text2, constants.Dir.BACKWARD);
      assertEquals(result.length, 1);
      assertEquals(result[0], text1);

      result = NodeNavigationUtils.getNextNodesInParagraph_(
          text1, constants.Dir.BACKWARD);
      assertEquals(result.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest', 'GetNodesForNextSentence',
    function() {
      const root = createMockNode({role: 'rootWebArea'});
      const paragraph1 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'Line 1.',
        sentenceStarts: [0],
      });
      const text2 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'Line 2.',
        sentenceStarts: [0],
      });
      const paragraph2 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text3 = createMockNode({
        role: 'staticText',
        parent: paragraph2,
        root,
        name: 'Line 3.',
        sentenceStarts: [0],
      });
      const text4 = createMockNode({
        role: 'staticText',
        parent: paragraph2,
        root,
        name: 'Line 4.',
        sentenceStarts: [0],
      });
      const nodeGroupForParagraph1 = ParagraphUtils.buildNodeGroup(
          [text1, text2], 0 /* index */, {splitOnLanguage: false});
      const nodeGroupForParagraph2 = ParagraphUtils.buildNodeGroup(
          [text3, text4], 0 /* index */, {splitOnLanguage: false});

      // First paragraph has two sentences.
      assertEquals(nodeGroupForParagraph1.text, 'Line 1. Line 2. ');
      // Second paragraph has another two sentences.
      assertEquals(nodeGroupForParagraph2.text, 'Line 3. Line 4. ');

      let nodes;
      let offset;
      // Navigating forward from the first sentence returns the second sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph1, 0 /* currentCharIndex */,
           constants.Dir.FORWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 1);
      assertEquals(nodes[0], text2);
      assertEquals(offset, 0);

      // Navigating forward from the second sentence returns the second
      // paragraph. Index 8 points to the word "Line" in the second sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph1, 8 /* currentCharIndex */,
           constants.Dir.FORWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 2);
      assertEquals(nodes[0], text3);
      assertEquals(nodes[1], text4);
      assertEquals(offset, undefined);

      // Navigating forward from the third sentence returns the fourth
      // sentence. Index 4 points to the word "3" in the third sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph2, 4 /* currentCharIndex */,
           constants.Dir.FORWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 1);
      assertEquals(nodes[0], text4);
      assertEquals(offset, 0);

      // Navigating forward from the fourth sentence returns an empty result.
      // Index 8 points to the word "Line" in the fourth sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph2, 8 /* currentCharIndex */,
           constants.Dir.FORWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 0);

      // Navigates forward from second sentence with a pred that filters out
      // paragraph 2. Index 8 points to the word "Line" in the second sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph1, 8 /* currentCharIndex */,
           constants.Dir.FORWARD,
           nodes => !(nodes.find(
               n => n.parent ===
                   paragraph2) /* filter out nodes belong to paragraph 2 */)));
      assertEquals(nodes.length, 0);

      // Navigating backward from the first sentence returns an empty result.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph1, 0 /* currentCharIndex */,
           constants.Dir.BACKWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 0);

      // Navigating backward from the second sentence returns the first
      // paragraph. Index 8 points to the word "Line" in the second sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph1, 8 /* currentCharIndex */,
           constants.Dir.BACKWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 2);
      assertEquals(nodes[0], text1);
      assertEquals(nodes[1], text2);
      assertEquals(offset, 0);

      // Navigating backward from the third sentence returns the second
      // sentence. Index 4 points to the word "3" in the third sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph2, 4 /* currentCharIndex */,
           constants.Dir.BACKWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 1);
      assertEquals(nodes[0], text2);
      assertEquals(offset, 0);

      // Navigating backward from the fourth sentence returns the second
      // paragraph. Index 8 points to the word "Line" in the fourth sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph2, 8 /* currentCharIndex */,
           constants.Dir.BACKWARD,
           () => true /* does not filter any paragraph */));
      assertEquals(nodes.length, 2);
      assertEquals(nodes[0], text3);
      assertEquals(nodes[1], text4);
      assertEquals(offset, 0);

      // Navigates backward from third sentence with a pred that filters out
      // paragraph 1.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroupForParagraph2, 0 /* currentCharIndex */,
           constants.Dir.BACKWARD,
           nodes => !(nodes.find(
               n => n.parent ===
                   paragraph1) /* filter out nodes belong to paragraph 1 */)));
      assertEquals(nodes.length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNodeNavigationUtilsUnitTest',
    'GetNodesForNextSentenceWithChoppedNodes', function() {
      const root = createMockNode({role: 'rootWebArea'});
      const paragraph1 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text1 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'Line 1.',
        sentenceStarts: [0],
      });
      const text2 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'This sentence',
        sentenceStarts: [0],
      });
      const text3 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'is chopped. Another',
        sentenceStarts: [12],
      });
      const text4 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'chopped',
        sentenceStarts: [],
      });
      const text5 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'sentence.',
        sentenceStarts: [],
      });
      const nodeGroup = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3, text4, text5], 0 /* index */,
          {splitOnLanguage: false});

      // One paragraph has three sentences.
      assertEquals(
          nodeGroup.text,
          'Line 1. This sentence is chopped. Another chopped sentence. ');

      let nodes;
      let offset;
      let result;
      // Navigating forward from the first word returns the content starting
      // from the second sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroup, 0 /* currentCharIndex */, constants.Dir.FORWARD,
           () => true /* does not filter any paragraph */));
      result = ParagraphUtils.buildNodeGroup(
          nodes, 0 /* index */, {splitOnLanguage: false});
      assertEquals(nodes.length, 4);
      assertEquals(offset, 0);
      assertEquals(
          result.text, 'This sentence is chopped. Another chopped sentence. ');

      // Navigating forward from the third word returns the content starting
      // from the third sentence. Index 8 points to the word "This" in the
      // second sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroup, 8 /* currentCharIndex */, constants.Dir.FORWARD,
           () => true /* does not filter any paragraph */));
      result = ParagraphUtils.buildNodeGroup(
          nodes, 0 /* index */, {splitOnLanguage: false});
      assertEquals(nodes.length, 3);
      assertEquals(offset, 12);
      assertEquals(result.text, 'is chopped. Another chopped sentence. ');

      // Navigating backward from the third sentence returns the content
      // starting from the second sentence. Index 42 points to the word
      // "chopped" in the third sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroup, 42 /* currentCharIndex */, constants.Dir.BACKWARD,
           () => true /* does not filter any paragraph */));
      result = ParagraphUtils.buildNodeGroup(
          nodes, 0 /* index */, {splitOnLanguage: false});
      assertEquals(nodes.length, 4);
      assertEquals(offset, 0);
      assertEquals(
          result.text, 'This sentence is chopped. Another chopped sentence. ');

      // Navigating backward from the third sentence returns the content
      // starting from the second sentence. Index 34 points to the word
      // "Another" in the third sentence.
      ({nodes, offset} = NodeNavigationUtils.getNodesForNextSentence(
           nodeGroup, 34 /* currentCharIndex */, constants.Dir.BACKWARD,
           () => true /* does not filter any paragraph */));
      result = ParagraphUtils.buildNodeGroup(
          nodes, 0 /* index */, {splitOnLanguage: false});
      assertEquals(nodes.length, 4);
      assertEquals(offset, 0);
      assertEquals(
          result.text, 'This sentence is chopped. Another chopped sentence. ');
    });

/**
 * Creates a nodeGroup that has an empty tail (i.e., "Hello world  ").
 * @return {!ParagraphUtils.NodeGroup}
 */
function generateTestNodeGroupWithEmptyTail() {
  const root = createMockNode({role: 'rootWebArea'});
  const paragraph =
      createMockNode({role: 'paragraph', display: 'block', parent: root, root});
  const text1 =
      createMockNode({name: 'Hello', role: 'staticText', parent: paragraph});
  const inlineText1 = createMockNode(
      {role: 'inlineTextBox', name: 'Hello', indexInParent: 0, parent: text1});

  const text2 =
      createMockNode({name: 'world  ', role: 'staticText', parent: paragraph});
  const inlineText2 = createMockNode(
      {role: 'inlineTextBox', name: 'world ', indexInParent: 0, parent: text2});
  const inlineText3 = createMockNode(
      {role: 'inlineTextBox', name: ' ', indexInParent: 1, parent: text2});

  return ParagraphUtils.buildNodeGroup(
      [inlineText1, inlineText2, inlineText3], 0,
      false /* do not split on language */);
}

/**
 * Creates a nodeGroup that has an empty head (i.e., "  Hello world").
 * @return {!ParagraphUtils.NodeGroup}
 */
function generateTestNodeGroupWithEmptyHead() {
  const root = createMockNode({role: 'rootWebArea'});
  const paragraph =
      createMockNode({role: 'paragraph', display: 'block', parent: root, root});
  const text1 =
      createMockNode({name: '  Hello', role: 'staticText', parent: paragraph});
  const inlineText1 = createMockNode(
      {role: 'inlineTextBox', name: ' ', indexInParent: 0, parent: text1});
  const inlineText2 = createMockNode(
      {role: 'inlineTextBox', name: ' Hello', indexInParent: 1, parent: text1});

  const text2 =
      createMockNode({name: 'world  ', role: 'staticText', parent: paragraph});
  const inlineText3 = createMockNode(
      {role: 'inlineTextBox', name: 'world', indexInParent: 1, parent: text2});

  return ParagraphUtils.buildNodeGroup(
      [inlineText1, inlineText2, inlineText3], 0,
      false /* do not split on language */);
}

/**
 * Creates a nodeGroup that only has a part of the paragraph (e.g., the "one"
 * in <p>Sentence <span>one</span> here</p>).
 * @return {!ParagraphUtils.NodeGroup}
 */
function generateTestNodeGroupFromPartialParagraph() {
  const root = createMockNode({role: 'rootWebArea'});
  const paragraph =
      createMockNode({role: 'paragraph', display: 'block', parent: root, root});
  const text1 =
      createMockNode({name: 'Sentence', role: 'staticText', parent: paragraph});

  const text2 =
      createMockNode({name: 'one', role: 'staticText', parent: paragraph});

  const text3 =
      createMockNode({name: 'here', role: 'staticText', parent: paragraph});

  return ParagraphUtils.buildNodeGroup(
      [text2], 0, false /* do not split on language */);
}
