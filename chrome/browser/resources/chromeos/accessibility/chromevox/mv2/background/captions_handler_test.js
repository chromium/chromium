// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for CaptionsHandler.
 */
ChromeVoxMV2CaptionsHandlerTest = class extends ChromeVoxE2ETest {};

AX_TEST_F('ChromeVoxMV2CaptionsHandlerTest', 'Open', function() {
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

AX_TEST_F('ChromeVoxMV2CaptionsHandlerTest', 'RangeObserver', async function() {
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
    'ChromeVoxMV2CaptionsHandlerTest', 'NoAttributeChangedEvent',
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
      assertEquals(goCount, 0);
      onAttributeChanged();
      assertEquals(goCount, 0);
    });

AX_TEST_F('ChromeVoxMV2CaptionsHandlerTest', 'MergeLines', function() {
  // Use braille captions as a fake braille display.
  BrailleCaptionsBackground.setActive(true);

  let getBrailleText =
      function() {
    return BrailleDisplayManager.instance.panStrategy_
        .getCurrentTextViewportContents();
  }

  // Simulates the text on live caption bubble.
  let lines = ['line1'];

  // First line shows up automatically.
  CaptionsHandler.instance.mergeLines_(lines);
  assertEquals('line1', getBrailleText());

  // First line gets updated but braille display will not.
  lines[0] += [' word2'];
  CaptionsHandler.instance.mergeLines_(lines);
  assertEquals('line1', getBrailleText());

  // Pan right and the next part of the first line shows up.
  CaptionsHandler.instance.next();
  assertEquals(' word2', getBrailleText());

  // First line is added and a 2nd line is created. No braille output yet.
  lines[0] += [' word3'];
  lines.push('line2');
  CaptionsHandler.instance.mergeLines_(lines);
  assertEquals(' word2', getBrailleText());

  // Pan right. And new word shows up.
  CaptionsHandler.instance.next();
  assertEquals(' word3', getBrailleText());

  // Pan right. And output is moved the 2nd line.
  CaptionsHandler.instance.next();
  assertEquals('line2', getBrailleText());

  // Adding more lines to make buffer almost overflow. Braille output is not
  // changed.
  for (let i = lines.length; i <= CaptionsHandler.MAX_LINES; ++i) {
    lines.push('line' + i);
  }
  CaptionsHandler.instance.mergeLines_(lines);
  assertEquals('line2', getBrailleText());

  // Push over the limit. The next line shows up on braille when buffer is
  // overflown.
  lines.push('line101');
  lines.push('line102');
  CaptionsHandler.instance.mergeLines_(lines);
  assertEquals('line3', getBrailleText());

  // Dropping input lines as caption lines scroll away.
  lines.splice(0, 10);
  CaptionsHandler.instance.mergeLines_(lines);

  // Output and buffer is not affected since first line in `lines` is in buffer.
  assertEquals('line3', getBrailleText());
  CaptionsHandler.instance.next();
  assertEquals('line4', getBrailleText());

  // Reset buffer if first line in `lines` could not be found in the buffer.
  lines = ['complete new'];
  CaptionsHandler.instance.mergeLines_(lines);

  // Output is updated in this case.
  assertEquals('complete new', getBrailleText());
});
