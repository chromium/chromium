// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../select_to_speak/select_to_speak_e2e_test_base.js']);

/**
 * Test fixture for sentence_utils.js.
 */
SelectToSpeakSentenceUtilsUnitTest = class extends SelectToSpeakE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('SentenceUtils', '/common/sentence_utils.js');
  }
};

AX_TEST_F(
    'SelectToSpeakSentenceUtilsUnitTest', 'getNextSentenceStart', function() {
      // The text of the test node group is "Hello. New. World."
      const nodeGroup = getTestNodeGroupWithOneNode();
      assertEquals(
          7,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 0 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          7,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 6 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          12,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 7 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          null,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 12 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          null,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 13 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
    });

AX_TEST_F(
    'SelectToSpeakSentenceUtilsUnitTest', 'getPrevSentenceStart', function() {
      // The text of the test node group is "Hello. New. World."
      const nodeGroup = getTestNodeGroupWithOneNode();

      assertEquals(
          null,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 0 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          0,
          SentenceUtils.getSentenceStart(
              nodeGroup, 6 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          0,
          SentenceUtils.getSentenceStart(
              nodeGroup, 7 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          7,
          SentenceUtils.getSentenceStart(
              nodeGroup, 12 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          12,
          SentenceUtils.getSentenceStart(
              nodeGroup, 13 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
    });

AX_TEST_F(
    'SelectToSpeakSentenceUtilsUnitTest', 'getNextSentenceStartMultiNodes',
    function() {
      // The text of the test node group is "Hello. New. Beautiful. World." The
      // char indexes of four sentence starts are 0, 7, 12, 23.
      const nodeGroup = getTestNodeGroupWithMultiNodes();

      assertEquals(
          7,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 0 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          7,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 6 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          12,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 7 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          23,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 12 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          null,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 23 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
    });

AX_TEST_F(
    'SelectToSpeakSentenceUtilsUnitTest', 'getPrevSentenceStartMultiNodes',
    function() {
      // The text of the test node group is "Hello. New. Beautiful. World." The
      // char indexes of four sentence starts are 0, 7, 12, 23.
      const nodeGroup = getTestNodeGroupWithMultiNodes();

      assertEquals(
          null,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 0 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          0,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 6 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          0,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 7 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          7,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 12 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          12,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 23 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
    });

AX_TEST_F(
    'SelectToSpeakSentenceUtilsUnitTest',
    'getNextSentenceStartSentenceSpanningAcrossMultiNodes', function() {
      // The text of the test node group is "Hello world. New world." The
      // char indexes of four sentence starts are 0, 13.
      const nodeGroup = getTestNodeGroupWithSentenceSpanningAcrossMultiNodes();

      assertEquals(
          13,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 0 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          13,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 6 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          13,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 11 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
      assertEquals(
          null,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 13 /* startCharIndex */,
              constants.Dir.FORWARD /* direction */));
    });

AX_TEST_F(
    'SelectToSpeakSentenceUtilsUnitTest',
    'getPrevSentenceStartSentenceSpanningAcrossMultiNodes', function() {
      // The text of the test node group is "Hello world. New world." The
      // char indexes of four sentence starts are 0, 13.
      const nodeGroup = getTestNodeGroupWithSentenceSpanningAcrossMultiNodes();

      assertEquals(
          null,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 0 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          0,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 6 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          0,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 11 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          0,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 13 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
      assertEquals(
          13,
          SentenceUtils.getSentenceStart(
              nodeGroup /* nodeGroup */, 15 /* startCharIndex */,
              constants.Dir.BACKWARD /* direction */));
    });

AX_TEST_F('SelectToSpeakSentenceUtilsUnitTest', 'isSentenceStart', function() {
  // The text of the test node group is "Hello. New. World."
  const nodeGroup = getTestNodeGroupWithOneNode();

  assertEquals(
      true,
      SentenceUtils.isSentenceStart(
          nodeGroup /* nodeGroup */, 0 /* startCharIndex */));
  assertEquals(
      false,
      SentenceUtils.isSentenceStart(
          nodeGroup /* nodeGroup */, 3 /* startCharIndex */));
  assertEquals(
      true,
      SentenceUtils.isSentenceStart(
          nodeGroup /* nodeGroup */, 7 /* startCharIndex */));
  assertEquals(
      false,
      SentenceUtils.isSentenceStart(
          nodeGroup /* nodeGroup */, 11 /* startCharIndex */));
  assertEquals(
      true,
      SentenceUtils.isSentenceStart(
          nodeGroup /* nodeGroup */, 12 /* startCharIndex */));
});

AX_TEST_F(
    'SelectToSpeakSentenceUtilsUnitTest', 'isSentenceStartMultiNodes',
    function() {
      // The text of the test node group is "Hello. New. Beautiful. World." The
      // char indexes of four sentence starts are 0, 7, 12, 23.
      const nodeGroup = getTestNodeGroupWithMultiNodes();

      assertEquals(
          true,
          SentenceUtils.isSentenceStart(
              nodeGroup /* nodeGroup */, 0 /* startCharIndex */));
      assertEquals(
          false,
          SentenceUtils.isSentenceStart(
              nodeGroup /* nodeGroup */, 3 /* startCharIndex */));
      assertEquals(
          true,
          SentenceUtils.isSentenceStart(
              nodeGroup /* nodeGroup */, 7 /* startCharIndex */));
      assertEquals(
          false,
          SentenceUtils.isSentenceStart(
              nodeGroup /* nodeGroup */, 11 /* startCharIndex */));
      assertEquals(
          true,
          SentenceUtils.isSentenceStart(
              nodeGroup /* nodeGroup */, 12 /* startCharIndex */));
      assertEquals(
          false,
          SentenceUtils.isSentenceStart(
              nodeGroup /* nodeGroup */, 15 /* startCharIndex */));
      assertEquals(
          true,
          SentenceUtils.isSentenceStart(
              nodeGroup /* nodeGroup */, 23 /* startCharIndex */));
    });
function getTestNodeGroupWithOneNode() {
  const staticText = {
    sentenceStarts: [0, 7, 12],
    name: 'Hello. New. World.',
    role: 'staticText',
  };
  const node = {node: staticText, startChar: 0};
  return {nodes: [node], text: 'Hello. New. World.'};
}

function getTestNodeGroupWithMultiNodes() {
  const staticText1 = {
    name: 'Hello. New. ',
    role: 'staticText',
    sentenceStarts: [0, 7],
  };
  const node1 = {node: staticText1, startChar: 0};

  const staticText2 = {
    name: 'Beautiful. World.',
    role: 'staticText',
    sentenceStarts: [0, 11],
  };
  const node2 = {node: staticText2, startChar: 12};

  return {nodes: [node1, node2], text: 'Hello. New. Beautiful. World.'};
}

function getTestNodeGroupWithSentenceSpanningAcrossMultiNodes() {
  const staticText1 = {name: 'Hello', role: 'staticText', sentenceStarts: [0]};
  const node1 = {node: staticText1, startChar: 0};

  const staticText2 = {
    name: ' world. New',
    role: 'staticText',
    sentenceStarts: [8],
  };
  const node2 = {node: staticText2, startChar: 5};

  const staticText3 = {name: ' world.', role: 'staticText', sentenceStarts: []};
  const node3 = {node: staticText3, startChar: 16};

  return {nodes: [node1, node2, node3], text: 'Hello world. New world.'};
}
