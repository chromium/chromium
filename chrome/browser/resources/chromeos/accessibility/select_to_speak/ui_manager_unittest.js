// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_accessibility_private.js']);

/** Focus ring color to use in testing. */
const FOCUS_RING_COLOR = '#ff0';

/** Highlight color to use in testing. */
const HIGHLIGHT_COLOR = '#00f';

/** Mock SelectToSpeakUiListener. */
class MockUiListener {
  constructor() {
    this.onNextParagraphRequestedCalled = false;
    this.onPreviousParagraphRequestedCalled = false;
    this.onNextSentenceRequestedCalled = false;
    this.onPreviousSentenceRequestedCalled = false;
    this.onPauseRequestedCalled = false;
    this.onResumeRequestedCalled = false;
    this.onChangeSpeedRequestedValue = undefined;
    this.onExitRequestedCalled = false;
    this.onStateChangeRequestedCalled = false;
  }

  onNextParagraphRequested() {
    this.onNextParagraphRequestedCalled = true;
  }

  onPreviousParagraphRequested() {
    this.onPreviousParagraphRequestedCalled = true;
  }

  onNextSentenceRequested() {
    this.onNextSentenceRequestedCalled = true;
  }

  onPreviousSentenceRequested() {
    this.onPreviousSentenceRequestedCalled = true;
  }

  onPauseRequested() {
    this.onPauseRequestedCalled = true;
  }

  onResumeRequested() {
    this.onResumeRequestedCalled = true;
  }

  onChangeSpeedRequested(speed) {
    this.onChangeSpeedRequestedValue = speed;
  }

  onExitRequested() {
    this.onExitRequestedCalled = true;
  }

  onStateChangeRequested() {
    this.onStateChangeRequestedCalled = true;
  }
}

/** Mock PrefsManager. Currently just returns hard-coded values.  */
class MockPrefsManager {
  backgroundShadingEnabled() {
    return true;
  }

  focusRingColor() {
    return FOCUS_RING_COLOR;
  }

  highlightColor() {
    return HIGHLIGHT_COLOR;
  }
}

/**
 * Test fixture for ui_manager.js.
 */
SelectToSpeakUiManagerUnitTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockAccessibilityPrivate = new MockAccessibilityPrivate();
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    this.mockPrefsManager = null;
    this.mockListener = null;
    this.uiManager = null;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    this.mockPrefsManager = new MockPrefsManager();
    this.mockListener = new MockUiListener();
    this.uiManager = new UiManager(this.mockPrefsManager, this.mockListener);
  }
};

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'NextParagraphActionCallsListener',
    function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction(
          'nextParagraph');
      assertTrue(this.mockListener.onNextParagraphRequestedCalled);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'PreviousParagraphActionCallsListener',
    function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction(
          'previousParagraph');
      assertTrue(this.mockListener.onPreviousParagraphRequestedCalled);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'NextSentenceActionCallsListener',
    function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction(
          'nextSentence');
      assertTrue(this.mockListener.onNextSentenceRequestedCalled);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'PreviousSentenceActionCallsListener',
    function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction(
          'previousSentence');
      assertTrue(this.mockListener.onPreviousSentenceRequestedCalled);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'PauseActionCallsListener', function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction('pause');
      assertTrue(this.mockListener.onPauseRequestedCalled);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'ResumeActionCallsListener', function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction('resume');
      assertTrue(this.mockListener.onResumeRequestedCalled);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'ChangeSpeedActionCallsListener',
    function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction(
          'changeSpeed', 1.2);
      assertEquals(1.2, this.mockListener.onChangeSpeedRequestedValue);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'ExitActionCallsListener', function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakPanelAction('exit');
      assertTrue(this.mockListener.onExitRequestedCalled);
    });

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'StateChangeCallsListener', function() {
      this.mockAccessibilityPrivate.sendSelectToSpeakStateChangeRequest();
      assertTrue(this.mockListener.onStateChangeRequestedCalled);
    });

AX_TEST_F('SelectToSpeakUiManagerUnitTest', 'SetSelectionRect', function() {
  const selectionRect = {left: 0, top: 10, width: 400, height: 200};

  // No focus rings to start.
  let focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(0, focusRings.length);

  this.uiManager.setSelectionRect(selectionRect);

  // Focus ring created to given rect.
  focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(1, focusRings.length);
  assertEquals(1, focusRings[0].rects.length);
  assertEquals(selectionRect, focusRings[0].rects[0]);
  assertEquals(FOCUS_RING_COLOR, focusRings[0].color);
});

AX_TEST_F('SelectToSpeakUiManagerUnitTest', 'UpdatesUi', function() {
  const textNode = {
    role: 'staticText',
    name: 'Test',
    location: {left: 20, top: 10, width: 100, height: 50},
  };
  const nodeGroup = ParagraphUtils.buildNodeGroup(
      [textNode], 0 /* index */, {splitOnLanguage: false});

  // No focus rings to start.
  let focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(0, focusRings.length);

  this.uiManager.update(
      nodeGroup, textNode, null,
      {showPanel: true, paused: false, speechRateMultiplier: 1});

  // Focus ring created highlighting the text node.
  focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(1, focusRings.length);
  assertEquals(1, focusRings[0].rects.length);
  assertEquals(textNode.location, focusRings[0].rects[0]);
  assertEquals(FOCUS_RING_COLOR, focusRings[0].color);

  // Panel created with correct state.
  const panelState = this.mockAccessibilityPrivate.getSelectToSpeakPanelState();
  assertTrue(panelState.show);
  assertEquals(textNode.location, panelState.anchor);
  assertFalse(panelState.isPaused);
  assertEquals(1, panelState.speed);

  // No highlights.
  const highlightRects = this.mockAccessibilityPrivate.getHighlightRects();
  assertEquals(0, highlightRects.length);
});

// This represents how Google Docs renders Canvas accessibility as of
// October 24 2022.
AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'UpdatesUiMultipleNodesInBlock',
    function() {
      const root = {
        role: 'svgRoot',
        location: {left: 0, top: 0, width: 500, height: 50},
      };
      const group = {
        role: 'paragraph',
        parent: root,
        display: 'inline',
        location: {left: 20, top: 10, width: 200, height: 10},
      };
      const text1 = {
        role: 'inlineTextBox',
        name: '1973',
        parent: group,
        location: {left: 20, top: 10, width: 100, height: 10},
      };
      const text2 = {
        role: 'inlineTextBox',
        name: 'Roe',
        parent: group,
        location: {left: 120, top: 10, width: 100, height: 10},
      };
      const nodeGroup = ParagraphUtils.buildNodeGroup(
          [text1, text2], /*index=*/ 0, {splitOnLanguage: false});

      assertEquals(group, nodeGroup.blockParent);

      this.uiManager.update(
          nodeGroup, text1, null,
          {showPanel: true, paused: false, speechRateMultiplier: 1});

      // When there are multiple nodes in a group, the parent should be
      // highlighted instead of the individual node.
      focusRings = this.mockAccessibilityPrivate.getFocusRings();
      assertEquals(1, focusRings.length);
      assertEquals(1, focusRings[0].rects.length);
      assertEquals(group.location, focusRings[0].rects[0]);
    });

AX_TEST_F('SelectToSpeakUiManagerUnitTest', 'UpdatesUiNoPanel', function() {
  const textNode = {
    role: 'staticText',
    name: 'Test',
    location: {left: 20, top: 10, width: 100, height: 50},
  };
  const nodeGroup = ParagraphUtils.buildNodeGroup(
      [textNode], 0 /* index */, {splitOnLanguage: false});

  // No focus rings to start.
  let focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(0, focusRings.length);

  this.uiManager.update(nodeGroup, textNode, null, {showPanel: false});

  // Focus ring created highlighting the text node.
  focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(1, focusRings.length);
  assertEquals(1, focusRings[0].rects.length);
  assertEquals(textNode.location, focusRings[0].rects[0]);
  assertEquals(FOCUS_RING_COLOR, focusRings[0].color);

  // No panel.
  const panelState = this.mockAccessibilityPrivate.getSelectToSpeakPanelState();
  assertFalse(panelState.show);
});

AX_TEST_F(
    'SelectToSpeakUiManagerUnitTest', 'UpdatesUiWithWordHighlight', function() {
      const wordBounds = {left: 25, top: 5, width: 20, height: 40};
      const wordStart = 0;
      const wordEnd = 5;
      const textNode = {
        role: 'staticText',
        name: 'Hello world',
        location: {left: 20, top: 10, width: 100, height: 50},
        boundsForRange: (start, end, callback) => {
          assertEquals(wordStart, start);
          assertEquals(wordEnd, end);
          callback(wordBounds);
        },
      };
      const nodeGroup = ParagraphUtils.buildNodeGroup(
          [textNode], 0 /* index */, {splitOnLanguage: false});

      // No highlights to start.
      let highlightRects = this.mockAccessibilityPrivate.getHighlightRects();
      assertEquals(0, highlightRects.length);

      this.uiManager.update(
          nodeGroup, textNode, {start: 0, end: 5},
          {showPanel: true, paused: false, speechRateMultiplier: 1});

      // Word highlight created.
      highlightRects = this.mockAccessibilityPrivate.getHighlightRects();
      assertEquals(1, highlightRects.length);
      assertEquals(wordBounds, highlightRects[0]);
      assertEquals(
          HIGHLIGHT_COLOR, this.mockAccessibilityPrivate.getHighlightColor());

      // AccessibilityPrivate informed of change.
      assertEquals(
          wordBounds, this.mockAccessibilityPrivate.getSelectToSpeakFocus());

      // Reset mockAccessibilityPrivate.
      this.mockAccessibilityPrivate.clearHighlightRects();
      this.mockAccessibilityPrivate.clearSelectToSpeakFocus();

      // When paused, AccessibilityPrivate is not informed of the change, but
      // highlights are still drawn visually.
      this.uiManager.update(
          nodeGroup, textNode, {start: 0, end: 5},
          {showPanel: true, paused: true, speechRateMultiplier: 1});

      highlightRects = this.mockAccessibilityPrivate.getHighlightRects();
      assertEquals(1, highlightRects.length);
      assertEquals(wordBounds, highlightRects[0]);
      assertEquals(
          HIGHLIGHT_COLOR, this.mockAccessibilityPrivate.getHighlightColor());
      assertEquals(null, this.mockAccessibilityPrivate.getSelectToSpeakFocus());
    });

AX_TEST_F('SelectToSpeakUiManagerUnitTest', 'ClearsUI', function() {
  // Start with a focus ring.
  this.uiManager.setSelectionRect({left: 0, top: 10, width: 400, height: 200});
  let focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(1, focusRings.length);

  this.uiManager.clear();

  // Focus rings are gone.
  focusRings = this.mockAccessibilityPrivate.getFocusRings();
  assertEquals(1, focusRings.length);
  assertEquals(0, focusRings[0].rects.length);

  // Panel is not visible.
  const panelState = this.mockAccessibilityPrivate.getSelectToSpeakPanelState();
  assertFalse(panelState.show);

  // No highlights.
  const highlightRects = this.mockAccessibilityPrivate.getHighlightRects();
  assertEquals(0, highlightRects.length);
});
