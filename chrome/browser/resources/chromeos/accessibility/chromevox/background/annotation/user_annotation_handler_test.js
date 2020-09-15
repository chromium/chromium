// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_next_e2e_test_base.js']);

GEN_INCLUDE(['../../testing/fake_objects.js']);

/**
 * Test fixture for UserAnnotationHandler.
 * @constructor
 * @extends {ChromeVoxE2ETest}
 */
ChromeVoxAnnotationTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
  #include "base/command_line.h"
  #include "ui/accessibility/accessibility_switches.h"
  #include "ui/base/ui_base_switches.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
    ::switches::kEnableExperimentalAccessibilityChromeVoxAnnotations);
    `);
    super.testGenPreamble();
  }

  assertNumberOfAnnotationsForUrl(url, numAnnotations) {
    const annotations = UserAnnotationHandler.instance.annotations_[url];
    if (!annotations) {
      console.error('No annotations for provided url');
      assertFalse(true);
    }
    assertEquals(numAnnotations, annotations.length);
  }

  /**
   * Returns the start node of the current ChromeVox range.
   * @return {AutomationNode}
   */
  getRangeStart() {
    return ChromeVoxState.instance.getCurrentRange().start.node;
  }

  /** @override */
  setUp() {
    window.doCmd = this.doCmd;
    window.RoleType = chrome.automation.RoleType;
  }

  // Test documents //
  get basicButtonDoc() {
    return `
    <p>Start here</p>
    <button>Apple</button>
    <button>Orange</button>
    `;
  }

  get duplicateButtonDoc() {
    return `
    <p>Start here</p>
    <button>Click me</button>
    <button>Click me</button>
    `;
  }
};

TEST_F('ChromeVoxAnnotationTest', 'BasicButtonTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.basicButtonDoc, function(root) {
    const pageUrl = root.docUrl;
    // Create annotations for both buttons.
    CommandHandler.onCommand('nextButton');
    const appleNode = this.getRangeStart();
    UserAnnotationHandler.setAnnotationForNode(appleNode, 'Batman');
    CommandHandler.onCommand('nextButton');
    const orangeNode = this.getRangeStart();
    UserAnnotationHandler.setAnnotationForNode(orangeNode, 'Robin');

    this.assertNumberOfAnnotationsForUrl(pageUrl, 2);
    assertEquals(
        UserAnnotationHandler.getAnnotationForNode(appleNode), 'Batman');
    assertEquals(
        UserAnnotationHandler.getAnnotationForNode(orangeNode), 'Robin');
    CommandHandler.onCommand('jumpToTop');
    mockFeedback.call(doCmd('nextButton'))
        .expectSpeech('Batman', 'Button')
        .call(doCmd('nextButton'))
        .expectSpeech('Robin', 'Button')
        .replay();
  });
});

TEST_F('ChromeVoxAnnotationTest', 'DuplicateButtonTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.duplicateButtonDoc, function(root) {
    const pageUrl = root.docUrl;
    CommandHandler.onCommand('nextButton');
    const firstButton = this.getRangeStart();
    UserAnnotationHandler.setAnnotationForNode(firstButton, 'First button');
    CommandHandler.onCommand('nextButton');
    const secondButton = this.getRangeStart();
    UserAnnotationHandler.setAnnotationForNode(secondButton, 'Second button');

    this.assertNumberOfAnnotationsForUrl(pageUrl, 2);
    assertEquals(
        UserAnnotationHandler.getAnnotationForNode(firstButton),
        'First button');
    assertEquals(
        UserAnnotationHandler.getAnnotationForNode(secondButton),
        'Second button');
    CommandHandler.onCommand('jumpToTop');
    mockFeedback.call(doCmd('nextButton'))
        .expectSpeech('First button', 'Button')
        .call(doCmd('nextButton'))
        .expectSpeech('Second button', 'Button')
        .replay();
  });
});

TEST_F('ChromeVoxAnnotationTest', 'UpdateAnnotationTest', function() {
  this.runWithLoadedTree(this.basicButtonDoc, function(root) {
    const pageUrl = root.docUrl;
    let appleButton =
        root.find({role: RoleType.BUTTON, attributes: {name: 'Apple'}});
    assertFalse(!appleButton);
    UserAnnotationHandler.setAnnotationForNode(appleButton, 'Good morning');
    this.assertNumberOfAnnotationsForUrl(pageUrl, 1);
    assertEquals(
        'Good morning',
        UserAnnotationHandler.getAnnotationForNode(appleButton));
    // Update annotation.
    appleButton =
        root.find({role: RoleType.BUTTON, attributes: {name: 'Apple'}});
    assertFalse(!appleButton);
    UserAnnotationHandler.setAnnotationForNode(appleButton, 'Good night');
    this.assertNumberOfAnnotationsForUrl(root.docUrl, 1);
    assertEquals(
        'Good night', UserAnnotationHandler.getAnnotationForNode(appleButton));
  });
});

// Tests that we can create an annotation for a node using its NodeIdentifier.
TEST_F('ChromeVoxAnnotationTest', 'CreateFromNodeIdentifier', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.basicButtonDoc, function(root) {
    const pageUrl = root.docUrl;
    const appleButton =
        root.find({role: RoleType.BUTTON, attributes: {name: 'Apple'}});
    assertFalse(!appleButton);
    const appleId = NodeIdentifier.constructFromNode(appleButton);
    UserAnnotationHandler.setAnnotationForIdentifier(appleId, 'Testing');

    this.assertNumberOfAnnotationsForUrl(pageUrl, 1);
    CommandHandler.onCommand('jumpToTop');
    mockFeedback.call(doCmd('nextButton'))
        .expectSpeech('Testing', 'Button')
        .call(doCmd('nextButton'))
        .expectSpeech('Orange', 'Button')
        .replay();
  });
});
