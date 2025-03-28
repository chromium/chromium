// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for CaptionsHandler.
 */
ChromeVoxCaptionsHandlerTest = class extends ChromeVoxE2ETest {};

AX_TEST_F('ChromeVoxCaptionsHandlerTest', 'Open', function() {
  let liveCaptionEnabledCount = 0;
  chrome.accessibilityPrivate.enableLiveCaption = () =>
      liveCaptionEnabledCount++;

  // Simulate the preference being false beforehand.
  SettingsManager.getBoolean = () => false;

  CaptionsHandler.open();
  assertEquals(1, liveCaptionEnabledCount);

  // Simulate the preference being true beforehand.
  SettingsManager.getBoolean = () => true;

  liveCaptionEnabledCount = 0;
  CaptionsHandler.open();
  assertEquals(0, liveCaptionEnabledCount);
});

AX_TEST_F('ChromeVoxCaptionsHandlerTest', 'RangeObserver', async function() {
  const root =
      await this.runWithLoadedTree('<button>Hello</button><p>World</p>');
  const button = root.find({role: 'button'});
  const text = root.find({attributes: {name: 'World'}});

  assertTrue(Boolean(button));
  assertTrue(Boolean(text));

  // Case: Range changes when focus is in captions.
  CaptionsHandler.instance.onEnterCaptions_();
  assertTrue(CaptionsHandler.instance.inCaptions_);
  ChromeVoxRange.navigateTo(CursorRange.fromNode(text));
  assertFalse(CaptionsHandler.instance.inCaptions_);
});


AX_TEST_F(
    'ChromeVoxCaptionsHandlerTest', 'HandleOnlyFirstAttributeChangedEvent',
    async function() {
      const desktop =
          await new Promise(resolve => this.runWithLoadedDesktop(resolve));

      let goCount = 0;
      Output.prototype.go = () => goCount++;

      CaptionsHandler.instance.inCaptions_ = true;
      ChromeVoxRange.instance.current_ = CursorRange.fromNode(desktop);

      const onAttributeChanged = () =>
          RangeAutomationHandler.instance.onAttributeChanged(
              new CustomAutomationEvent('name', desktop));

      onAttributeChanged();
      assertEquals(goCount, 1);
      onAttributeChanged();
      assertEquals(goCount, 1);
    });
