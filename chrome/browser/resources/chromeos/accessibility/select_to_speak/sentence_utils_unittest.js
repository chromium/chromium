// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for sentence_utils.js.
 */
SelectToSpeakSentenceUtilsUnitTest = class extends testing.Test {};

/** @override */
SelectToSpeakSentenceUtilsUnitTest.prototype.extraLibraries = [
  'test_support.js',
  'sentence_utils.js',
  'paragraph_utils.js',
  '../common/closure_shim.js',
  '../common/constants.js',
];

TEST_F(
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

TEST_F(
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

TEST_F(
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

TEST_F(
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

TEST_F(
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

TEST_F(
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

function getTestNodeGroupWithOneNode() {
  const inlineText = {sentenceStarts: [0, 7, 12], name: 'Hello. New. World.'};
  const staticText = {children: [inlineText], name: 'Hello. New. World.'};
  const node = {node: staticText, startChar: 0, hasInlineText: true};
  return {nodes: [node], text: 'Hello. New. World.'};
}

function getTestNodeGroupWithMultiNodes() {
  const staticText1 = {name: 'Hello. New. ', role: 'staticText'};
  const inlineText1 = {
    sentenceStarts: [0, 7],
    name: 'Hello. New. ',
    indexInParent: 0,
    parent: staticText1
  };
  staticText1.children = [inlineText1];
  const node1 = {node: staticText1, startChar: 0, hasInlineText: true};

  const staticText2 = {name: 'Beautiful. World.', role: 'staticText'};
  const inlineText2 = {
    sentenceStarts: [0],
    name: 'Beautiful. ',
    indexInParent: 0,
    parent: staticText2
  };
  const inlineText3 = {
    sentenceStarts: [0],
    name: 'World.',
    indexInParent: 1,
    parent: staticText2
  };
  staticText2.children = [inlineText2, inlineText3];
  const node2 = {node: staticText2, startChar: 12, hasInlineText: true};

  return {nodes: [node1, node2], text: 'Hello. New. Beautiful. World.'};
}

function getTestNodeGroupWithSentenceSpanningAcrossMultiNodes() {
  const staticText1 = {name: 'Hello', role: 'staticText'};
  const inlineText1 = {
    sentenceStarts: [0],
    name: 'Hello',
    indexInParent: 0,
    parent: staticText1
  };
  staticText1.children = [inlineText1];
  const node1 = {node: staticText1, startChar: 0, hasInlineText: true};

  const staticText2 = {name: ' world. New', role: 'staticText'};
  const inlineText2 = {
    sentenceStarts: [],
    name: ' world.',
    indexInParent: 0,
    parent: staticText2
  };
  const inlineText3 = {
    sentenceStarts: [1],
    name: ' New',
    indexInParent: 1,
    parent: staticText2
  };
  staticText2.children = [inlineText2, inlineText3];
  const node2 = {node: staticText2, startChar: 5, hasInlineText: true};

  const staticText3 = {name: ' world.', role: 'staticText'};
  const inlineText4 = {
    sentenceStarts: [],
    name: ' world.',
    indexInParent: 0,
    parent: staticText3
  };
  staticText3.children = [inlineText4];
  const node3 = {node: staticText3, startChar: 16, hasInlineText: true};

  return {nodes: [node1, node2, node3], text: 'Hello world. New world.'};
}