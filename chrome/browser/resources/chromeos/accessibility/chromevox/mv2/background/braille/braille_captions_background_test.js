// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/** Test fixture. */
ChromeVoxBrailleCaptionsBackgroundTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    /** @implements BrailleCaptionsListener */
    this.listener = new class {
      constructor() {
        this.captionsChangedCount = 0;
      }

      onBrailleCaptionsStateChanged() {
        this.captionsChangedCount++;
      }
    }
    ();
    delete BrailleCaptionsBackground.instance;
    BrailleCaptionsBackground.init(this.listener);
    this.captions = BrailleCaptionsBackground.instance;
  }
};

AX_TEST_F(
    'ChromeVoxBrailleCaptionsBackgroundTest', 'EnableAndDisable',
    async function() {
      // Default should be disabled.
      assertFalse(BrailleCaptionsBackground.isEnabled());

      LocalStorage.set('brailleCaptions', true);
      assertTrue(BrailleCaptionsBackground.isEnabled());

      LocalStorage.set('brailleCaptions', false);
      assertFalse(BrailleCaptionsBackground.isEnabled());
    });

AX_TEST_F(
    'ChromeVoxBrailleCaptionsBackgroundTest', 'SetContent', async function() {
      // Override PanelCommand.send() to capture the braille displayed.
      globalThis.setContentOutput = null;
      // Use a function() rather than an arrow lambda to allow access to |this|.
      PanelCommand.prototype.send = function() {
        globalThis.setContentOutput = this.data;
      };

      const text = 'Hello!';
      const cells = new Uint8Array([0x53, 0x11, 0x07, 0x07, 0x15, 0x2e]).buffer;
      const brailleToText = [0, 1, 2, 3, 4, 5];
      const offsetForSlices = {brailleOffset: 0, textOffset: 0};
      const rows = 1;
      const columns = 6;

      BrailleCaptionsBackground.setContent(
          text, cells, brailleToText, offsetForSlices, rows, columns);

      assertEquals(rows, setContentOutput.rows);
      assertEquals(columns, setContentOutput.cols);

      const expectedGroups = [
        ['H', '\u2853'],
        ['e', '\u2811'],
        ['l', '\u2807'],
        ['l', '\u2807'],
        ['o', '\u2815'],
        ['!', '\u282e'],
      ];
      assertEquals(6, setContentOutput.groups.length);

      for (const i of brailleToText) {
        expectedText = expectedGroups[i][0];
        expectedBraille = expectedGroups[i][1];
        receivedText = setContentOutput.groups[i][0];
        receivedBraille = setContentOutput.groups[i][1];

        assertEquals(expectedText, receivedText);
        assertEquals(expectedBraille, receivedBraille);
      }
    });

AX_TEST_F(
    'ChromeVoxBrailleCaptionsBackgroundTest', 'SetImageContent',
    async function() {
      // Override PanelCommand.send() to capture the braille displayed.
      globalThis.setImageContentOutput = null;
      // Use a function() rather than an arrow lambda to allow access to |this|.
      PanelCommand.prototype.send = function() {
        globalThis.setImageContentOutput = this.data;
      };

      const cells = new Uint8Array([0x01, 0x02, 0x03]).buffer;
      const rows = 1;
      const columns = 3;

      BrailleCaptionsBackground.setImageContent(cells, rows, columns);

      assertEquals(rows, setImageContentOutput.rows);
      assertEquals(columns, setImageContentOutput.cols);
      assertEquals('Image', setImageContentOutput.groups[0][0]);
      assertEquals('\u2801\u2802\u2803', setImageContentOutput.groups[0][1]);
    });

AX_TEST_F(
    'ChromeVoxBrailleCaptionsBackgroundTest', 'SetActive', async function() {
      const checkMethodCallsForValue = (value, changed) => {
        this.addCallbackPostMethod(
            ChromeVoxPrefs.instance, 'setPref', (key, val) => {
              assertEquals('brailleCaptions', key);
              assertEquals(value, val);
            }, /*reset*/ () => true);

        if (changed) {
          this.addCallbackPostMethod(
              Msgs, 'getMsg',
              (name) => assertEquals(
                  value ? 'braille_captions_enabled' :
                          'braille_captions_disabled'),
              /*reset*/ () => true);
          this.addCallbackPostMethod(
              ChromeVox.tts, 'speak',
              (msg, queueMode) => assertEquals(QueueMode.QUEUE, queueMode),
              /*reset*/ () => true);
          this.addCallbackPostMethod(
              ChromeVox.braille, 'write', this.newCallback(),
              /*reset*/ () => true);
        }
      };

      // Enable when disabled.
      assertFalse(BrailleCaptionsBackground.isEnabled());
      assertEquals(0, this.listener.captionsChangedCount);
      checkMethodCallsForValue(true, /*changed*/ true);

      BrailleCaptionsBackground.setActive(true);
      assertEquals(1, this.listener.captionsChangedCount);

      // Enable when enabled.
      await this.waitForPendingMethods();
      assertTrue(BrailleCaptionsBackground.isEnabled());
      checkMethodCallsForValue(true, /*changed*/ false);

      BrailleCaptionsBackground.setActive(true);
      assertEquals(1, this.listener.captionsChangedCount);

      // Disable when enabled.
      await this.waitForPendingMethods();
      assertTrue(BrailleCaptionsBackground.isEnabled());
      checkMethodCallsForValue(false, /*changed*/ true);

      BrailleCaptionsBackground.setActive(false);
      assertEquals(2, this.listener.captionsChangedCount);

      // Disable when disabled.
      await this.waitForPendingMethods();
      assertFalse(BrailleCaptionsBackground.isEnabled());
      checkMethodCallsForValue(false, /*changed*/ false);

      BrailleCaptionsBackground.setActive(false);
      assertEquals(2, this.listener.captionsChangedCount);
    });

AX_TEST_F(
    'ChromeVoxBrailleCaptionsBackgroundTest', 'GetVirtualDisplayState',
    async function() {
      // Expect a default result when the display is disabled.
      assertFalse(BrailleCaptionsBackground.isEnabled());

      let displayState = BrailleCaptionsBackground.getVirtualDisplayState();
      assertFalse(displayState.available);
      assertEquals(0, displayState.textRowCount);
      assertEquals(0, displayState.textColumnCount);
      assertEquals(0, displayState.cellSize);

      // Expect to receive the current row and columns when enabled.
      LocalStorage.set('brailleCaptions', true);
      assertTrue(BrailleCaptionsBackground.isEnabled());

      displayState = BrailleCaptionsBackground.getVirtualDisplayState();
      assertTrue(displayState.available);
      assertEquals(1, displayState.textRowCount);
      assertEquals(40, displayState.textColumnCount);
      assertEquals(8, displayState.cellSize);
    });
