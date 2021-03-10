// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);

/**
 * Test fixture for navigation_utils.js.
 */
SelectToSpeakNodeNavigationUtilsUnitTest = class extends SelectToSpeakE2ETest {
  /** @override */
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async function() {
      let module = await import('/select_to_speak/node_navigation_utils.js');
      window.NodeNavigationUtils = module.NodeNavigationUtils;

      module = await import('/select_to_speak/node_utils.js');
      window.NodeUtils = module.NodeUtils;

      module = await import('/select_to_speak/paragraph_utils.js');
      window.ParagraphUtils = module.ParagraphUtils;

      module = await import('/select_to_speak/sentence_utils.js');
      runTest();
    })();
  }
};

SYNC_TEST_F(
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
          (nodes) => !(nodes.find(
              n => n.parent ===
                  paragraph2) /* filter out nodes belong to paragraph 2 */));
      assertEquals(result.length, 0);
    });

SYNC_TEST_F(
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
        sentenceStarts: [0]
      });
      const text2 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'Line 2.',
        sentenceStarts: [0]
      });
      const paragraph2 = createMockNode(
          {role: 'paragraph', display: 'block', parent: root, root});
      const text3 = createMockNode({
        role: 'staticText',
        parent: paragraph2,
        root,
        name: 'Line 3.',
        sentenceStarts: [0]
      });
      const text4 = createMockNode({
        role: 'staticText',
        parent: paragraph2,
        root,
        name: 'Line 4.',
        sentenceStarts: [0]
      });
      const nodeGroupForParagraph1 = ParagraphUtils.buildNodeGroup(
          [text1, text2], 0 /* index */, {splitOnLanguage: false});
      const nodeGroupForParagraph2 = ParagraphUtils.buildNodeGroup(
          [text3, text4], 0 /* index */, {splitOnLanguage: false});

      // First paragraph has two sentences.
      assertEquals(nodeGroupForParagraph1.text, 'Line 1. Line 2. ');
      // Second paragraph has another two sentences.
      assertEquals(nodeGroupForParagraph2.text, 'Line 3. Line 4. ');

      let nodes, offset;
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
           (nodes) => !(nodes.find(
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
           (nodes) => !(nodes.find(
               n => n.parent ===
                   paragraph1) /* filter out nodes belong to paragraph 1 */)));
      assertEquals(nodes.length, 0);
    });

SYNC_TEST_F(
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
        sentenceStarts: [0]
      });
      const text2 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'This sentence',
        sentenceStarts: [0]
      });
      const text3 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'is chopped. Another',
        sentenceStarts: [12]
      });
      const text4 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'chopped',
        sentenceStarts: []
      });
      const text5 = createMockNode({
        role: 'staticText',
        parent: paragraph1,
        root,
        name: 'sentence.',
        sentenceStarts: []
      });
      const nodeGroup = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3, text4, text5], 0 /* index */,
          {splitOnLanguage: false});

      // One paragraph has three sentences.
      assertEquals(
          nodeGroup.text,
          'Line 1. This sentence is chopped. Another chopped sentence. ');

      let nodes, offset, result;
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
 * Creates a AutomationNode-like object.
 * @param {!Object} properties
 */
function createMockNode(properties) {
  const node = Object.assign(
      {
        htmlAttributes: [],
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