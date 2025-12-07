// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for MathHandler.
 */
ChromeVoxMV2MathHandlerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    const imports = TestImportManager.getImports();
    globalThis.MathHandler = imports.MathHandler;
  }

  createMockNode(properties) {
    return Object.assign(
        {
          state: {},
          children: [],
          unclippedLocation: {left: 20, top: 10, width: 100, height: 50},
          location: {left: 20, top: 10, width: 100, height: 50},
        },
        properties);
  }
};

AX_TEST_F(
    'ChromeVoxMV2MathHandlerTest', 'noMathRootInMathContent', async function() {
      const mockFeedback = this.createMockFeedback();
      const math = this.createMockNode({
        role: chrome.automation.RoleType.MathMlMath,
        mathContent:
            '<mfrac><mrow><mi>d</mi><mi>y</mi></mrow><mrow><mi>d</mi>' +
            '<mi>x</mi></mrow></mfrac><mo>=</mo>'
      });
      const range = CursorRange.fromNode(math);
      MathHandler.init(range);
      mockFeedback.call(() => MathHandler.instance.speak())
          .expectSpeech('StartFraction d y Over d x EndFraction =')
          .expectSpeech('Press up, down, left, or right to explore math');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxMV2MathHandlerTest', 'mathRootInMathContent', async function() {
      const mockFeedback = this.createMockFeedback();
      const math = this.createMockNode({
        role: chrome.automation.RoleType.MathMlMath,
        mathContent: '<math><mfrac><mrow><mi>d</mi><mi>y</mi></mrow><mrow>' +
            '<mi>d</mi><mi>x</mi></mrow></mfrac><mo>=</mo></math>'
      });
      const range = CursorRange.fromNode(math);
      MathHandler.init(range);
      mockFeedback.call(() => MathHandler.instance.speak())
          .expectSpeech('StartFraction d y Over d x EndFraction =')
          .expectSpeech('Press up, down, left, or right to explore math');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxMV2MathHandlerTest', 'mathRootDoesntMatchRegexInMathContent',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const math = this.createMockNode({
        role: chrome.automation.RoleType.MathMlMath,
        mathContent: '<math id="42"><mfrac><mrow><mi>d</mi><mi>y</mi></mrow>' +
            '<mrow><mi>d</mi><mi>x</mi></mrow></mfrac><mo>=</mo></math>'
      });
      const range = CursorRange.fromNode(math);
      MathHandler.init(range);
      mockFeedback.call(() => MathHandler.instance.speak())
          .expectSpeech('StartFraction d y Over d x EndFraction =')
          .expectSpeech('Press up, down, left, or right to explore math');
      await mockFeedback.replay();
    });
