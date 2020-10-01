// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for SmartStickyMode.
 */
ChromeVoxSmartStickyModeTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    window.doCmd = this.doCmd;
    this.ssm_ = new SmartStickyMode();
    // Deregister from actual range changes.
    ChromeVoxState.removeObserver(this.ssm_);
    assertFalse(this.ssm_.didTurnOffStickyMode_);
  }

  assertDidTurnOffForNode(node) {
    this.ssm_.onCurrentRangeChanged(cursors.Range.fromNode(node));
    assertTrue(this.ssm_.didTurnOffStickyMode_);
  }

  assertDidNotTurnOffForNode(node) {
    this.ssm_.onCurrentRangeChanged(cursors.Range.fromNode(node));
    assertFalse(this.ssm_.didTurnOffStickyMode_);
  }

  get relationsDoc() {
    return `
      <p>start</p>
      <input aria-controls="controls-target" type="text"></input>
      <textarea aria-activedescendant="active-descendant-target"></textarea>
      <div contenteditable><h3>hello</h3></div>
      <ul id="controls-target"><li>end</ul>
      <ul id="active-descendant-target"><li>end</ul>
    `;
  }
};

TEST_F('ChromeVoxSmartStickyModeTest', 'PossibleRangeTypes', function() {
  this.runWithLoadedTree(this.relationsDoc, function(root) {
    const [p, input, textarea, contenteditable, ul1, ul2] = root.children;

    // First, turn on sticky mode and try changing range to various parts of
    // the document.
    ChromeVoxBackground.setPref(
        'sticky', true /* value */, true /* announce */);
    this.assertDidTurnOffForNode(input);
    this.assertDidTurnOffForNode(textarea);
    this.assertDidNotTurnOffForNode(p);
    this.assertDidTurnOffForNode(contenteditable);
    this.assertDidTurnOffForNode(ul1);
    this.assertDidNotTurnOffForNode(p);
    this.assertDidTurnOffForNode(ul2);
    this.assertDidTurnOffForNode(ul1.firstChild);
    this.assertDidNotTurnOffForNode(ul1.parent);
    this.assertDidNotTurnOffForNode(ul2.parent);
    this.assertDidNotTurnOffForNode(p);
    this.assertDidTurnOffForNode(ul2.firstChild);
    this.assertDidNotTurnOffForNode(p);
    this.assertDidNotTurnOffForNode(contenteditable.parent);
    this.assertDidTurnOffForNode(contenteditable.find({role: 'heading'}));
    this.assertDidTurnOffForNode(contenteditable.find({role: 'inlineTextBox'}));
  });
});

TEST_F(
    'ChromeVoxSmartStickyModeTest', 'UserPressesStickyModeCommand', function() {
      this.runWithLoadedTree(this.relationsDoc, function(root) {
        const [p, input, textarea, contenteditable, ul1, ul2] = root.children;
        ChromeVoxBackground.setPref(
            'sticky', true /* value */, true /* announce */);

        // Mix in calls to turn on / off sticky mode while moving the range
        // around.
        this.assertDidTurnOffForNode(input);
        this.ssm_.onStickyModeCommand(cursors.Range.fromNode(input));
        this.assertDidNotTurnOffForNode(input);
        this.ssm_.onStickyModeCommand(cursors.Range.fromNode(input));
        this.assertDidNotTurnOffForNode(input);
        this.assertDidNotTurnOffForNode(input.firstChild);
        this.assertDidNotTurnOffForNode(p);

        // Make sure sticky mode is on again. This call doesn't impact our
        // instance of SmartStickyMode.
        ChromeVoxBackground.setPref(
            'sticky', true /* value */, true /* announce */);

        // Mix in more sticky mode user commands and move to related nodes.
        this.assertDidTurnOffForNode(contenteditable);
        this.assertDidTurnOffForNode(ul2);
        this.ssm_.onStickyModeCommand(cursors.Range.fromNode(ul2));
        this.assertDidNotTurnOffForNode(ul2);
        this.assertDidNotTurnOffForNode(ul2.firstChild);
        this.assertDidNotTurnOffForNode(contenteditable);
        this.ssm_.onStickyModeCommand(cursors.Range.fromNode(input));
        this.assertDidNotTurnOffForNode(ul2);
        this.assertDidNotTurnOffForNode(ul2.firstChild);
        this.assertDidNotTurnOffForNode(contenteditable);

        // Finally, verify sticky mode isn't impacted on non-editables.
        this.assertDidNotTurnOffForNode(p);
        this.ssm_.onStickyModeCommand(cursors.Range.fromNode(p));
        this.assertDidNotTurnOffForNode(p);
        this.ssm_.onStickyModeCommand(cursors.Range.fromNode(p));
        this.assertDidNotTurnOffForNode(p);
      });
    });

TEST_F(
    'ChromeVoxSmartStickyModeTest', 'SmartStickyModeJumpCommands', function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          `
        <p>start</p>
        <input type="text"></input>
        <button>end</button>
      `,
          function(root) {
            mockFeedback.call(doCmd('toggleStickyMode'))
                .expectSpeech('Sticky mode enabled')
                .call(doCmd('nextFormField'))
                .expectSpeech('Edit text')
                .call(() => assertTrue(ChromeVox.isStickyModeOn()))
                .call(doCmd('nextFormField'))
                .expectSpeech('Button')
                .call(doCmd('previousFormField'))
                .expectSpeech('Edit text')
                .call(() => assertTrue(ChromeVox.isStickyModeOn()))
                .call(doCmd('previousObject'))
                .expectSpeech('start')
                .call(doCmd('nextEditText'))
                .expectSpeech('Edit text')
                .call(() => assertTrue(ChromeVox.isStickyModeOn()))
                .call(doCmd('nextObject'))
                .expectSpeech('Button')
                .call(doCmd('previousEditText'))
                .expectSpeech('Edit text')
                .call(() => assertTrue(ChromeVox.isStickyModeOn()))
                .call(doCmd('nextObject'))
                .expectSpeech('Button')
                .call(doCmd('previousObject'))
                .expectSpeech('Sticky mode disabled')
                .expectSpeech('Edit text')
                .call(() => assertFalse(ChromeVox.isStickyModeOn()))
                .replay();
          });
    });

TEST_F('ChromeVoxSmartStickyModeTest', 'SmartStickyModeEarcons', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <p>start</p>
    <input type="text"></input>
    <button>end</button>
  `,
      function(root) {
        mockFeedback.call(doCmd('toggleStickyMode'))
            .expectSpeech('Sticky mode enabled')
            .call(doCmd('nextObject'))
            .expectEarcon(Earcon.SMART_STICKY_MODE_OFF)
            .expectSpeech('Sticky mode disabled')
            .expectSpeech('Edit text')
            .call(() => assertFalse(ChromeVox.isStickyModeOn()))
            .call(doCmd('nextObject'))
            .expectEarcon(Earcon.SMART_STICKY_MODE_ON)
            .expectSpeech('Sticky mode enabled')
            .expectSpeech('Button')
            .call(() => assertTrue(ChromeVox.isStickyModeOn()))
            .replay();
      });
});
