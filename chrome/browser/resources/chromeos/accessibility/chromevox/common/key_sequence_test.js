// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture.
 */
ChromeVoxKeySequenceUnitTest = class extends ChromeVoxE2ETest {
  /**
   * Create mock event object.
   * @param {number} keyCode The event key code (i.e. 13 for Enter).
   * @param {{altGraphKey: boolean=,
   *         altKey: boolean=,
   *         ctrlKey: boolean=,
   *         metaKey: boolean=,
   *         searchKeyHeld: boolean=,
   *         shiftKey: boolean=,
   *         stickyMode: boolean=,
   *         prefixKey: boolean=}} eventParams The parameters on the event.
   *  altGraphKey: Whether or not the altGraph key was held down.
   *  altKey: Whether or not the alt key was held down.
   *  ctrlKey: Whether or not the ctrl key was held down.
   *  metaKey: Whether or not the meta key was held down.
   *  searchKeyHeld: Whether or not the search key was held down.
   *  shiftKey: Whether or not the shift key was held down.
   *  stickyMode: Whether or not sticky mode is enabled.
   *  prefixKey: Whether or not the prefix key was entered.
   * @return {Object} The mock event.
   */
  createMockEvent(keyCode, eventParams) {
    const mockEvent = {};
    mockEvent.keyCode = keyCode;

    if (eventParams == null) {
      return mockEvent;
    }
    if (eventParams.hasOwnProperty('altGraphKey')) {
      mockEvent.altGraphKey = eventParams.altGraphKey;
    }
    if (eventParams.hasOwnProperty('altKey')) {
      mockEvent.altKey = eventParams.altKey;
    }
    if (eventParams.hasOwnProperty('ctrlKey')) {
      mockEvent.ctrlKey = eventParams.ctrlKey;
    }
    if (eventParams.hasOwnProperty('metaKey')) {
      mockEvent.metaKey = eventParams.metaKey;
    }
    if (eventParams.hasOwnProperty('shiftKey')) {
      mockEvent.shiftKey = eventParams.shiftKey;
    }

    if (eventParams.hasOwnProperty('searchKeyHeld')) {
      mockEvent.searchKeyHeld = eventParams.searchKeyHeld;
    }
    if (eventParams.hasOwnProperty('stickyMode')) {
      mockEvent.stickyMode = eventParams.stickyMode;
    }
    if (eventParams.hasOwnProperty('prefixKey')) {
      mockEvent.keyPrefix = eventParams.prefixKey;
    }

    return mockEvent;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Set up mock ChromeVox modifier
    KeySequence.modKeyStr = 'Alt';

    // Use these mock events in the tests:

    // Down arrow, no modifiers
    this.downArrowEvent = this.createMockEvent(KeyCode.DOWN);

    // Down arrow key with alt held down. We specified 'Alt' as the
    // mock ChromeVox modifier string, so this means that KeySequence
    // should interpret this as the ChromeVox modifier being active.
    this.altDownArrowEvent = this.createMockEvent(KeyCode.DOWN, {altKey: true});

    // Right arrow, no modifiers
    this.rightArrowEvent = this.createMockEvent(KeyCode.RIGHT);

    // Ctrl key, no modifiers
    this.ctrlEvent = this.createMockEvent(KeyCode.CONTROL);

    // Ctrl key with sticky mode
    this.ctrlStickyEvent =
        this.createMockEvent(KeyCode.CONTROL, {stickyMode: true});

    // Ctrl key with prefix mode
    this.ctrlPrefixEvent =
        this.createMockEvent(KeyCode.CONTROL, {prefixKey: true});

    // 'a' key, no modifiers
    this.aEvent = this.createMockEvent(KeyCode.A);

    // 'a' key with ctrl held down
    this.ctrlAEvent = this.createMockEvent(KeyCode.A, {ctrlKey: true});

    // 'a' key with meta held down
    this.metaAEvent = this.createMockEvent(KeyCode.A, {metaKey: true});

    // 'a' key with shift held down
    this.shiftAEvent = this.createMockEvent(KeyCode.A, {shiftKey: true});

    // 'a' key with alt (which is the mock ChromeVox modifier) and shift held
    // down.
    this.altShiftAEvent =
        this.createMockEvent(KeyCode.A, {altKey: true, shiftKey: true});

    // 'a' key with shift and prefix held down
    this.shiftAPrefixEvent =
        this.createMockEvent(KeyCode.A, {shiftKey: true, prefixKey: true});

    // 'a' key with shift and sticky mode
    this.shiftAStickyEvent =
        this.createMockEvent(KeyCode.A, {shiftKey: true, stickyMode: true});

    // 'a' key with sticky mode
    this.aEventSticky = this.createMockEvent(KeyCode.A, {stickyMode: true});

    // 'a' key with prefix key
    this.aEventPrefix = this.createMockEvent(KeyCode.A, {prefixKey: true});

    // 'a' key with alt (which is the mock ChromeVox modifier) held down
    this.altAEvent = this.createMockEvent(KeyCode.A, {altKey: true});

    // 'b' key, no modifiers
    this.bEvent = this.createMockEvent(KeyCode.B);

    // 'b' key, with ctrl held down
    this.ctrlBEvent = this.createMockEvent(KeyCode.B, {ctrlKey: true});

    // 'c' key, no modifiers
    this.cEvent = this.createMockEvent(KeyCode.C);

    // Shift key with ctrl held down
    this.ctrlShiftEvent = this.createMockEvent(60, {ctrlKey: true});

    // Ctrl key with shift held down
    this.shiftCtrlEvent =
        this.createMockEvent(KeyCode.CONTROL, {shiftKey: true});
  }
};

AX_TEST_F(
    'ChromeVoxKeySequenceUnitTest', 'SimpleSequenceNoModifier', function() {
      const downKey = new KeySequence(this.downArrowEvent, false);

      assertEqualsJSON([KeyCode.DOWN], downKey.keys.keyCode);
      assertFalse(downKey.stickyMode);
      assertFalse(downKey.prefixKey);
      assertFalse(downKey.cvoxModifier);

      assertEqualsJSON([false], downKey.keys.altGraphKey);
      assertEqualsJSON([false], downKey.keys.altKey);
      assertEqualsJSON([false], downKey.keys.ctrlKey);
      assertEqualsJSON([false], downKey.keys.metaKey);
      assertEqualsJSON([false], downKey.keys.searchKeyHeld);
      assertEqualsJSON([false], downKey.keys.shiftKey);

      assertEquals(1, downKey.length());
    });


/** Test another key sequence, this time with the modifier */
AX_TEST_F(
    'ChromeVoxKeySequenceUnitTest', 'SimpleSequenceWithModifier', function() {
      const downKey = new KeySequence(this.downArrowEvent, true);

      assertEqualsJSON([KeyCode.DOWN], downKey.keys.keyCode);
      assertFalse(downKey.stickyMode);
      assertFalse(downKey.prefixKey);
      assertTrue(downKey.cvoxModifier);

      assertEqualsJSON([false], downKey.keys.altGraphKey);
      assertEqualsJSON([false], downKey.keys.altKey);
      assertEqualsJSON([false], downKey.keys.ctrlKey);
      assertEqualsJSON([false], downKey.keys.metaKey);
      assertEqualsJSON([false], downKey.keys.searchKeyHeld);
      assertEqualsJSON([false], downKey.keys.shiftKey);

      assertEquals(1, downKey.length());
    });


/** Test a key sequence that includes the modifier */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'ModifiedSequence', function() {
  const cvoxDownKey = new KeySequence(this.altDownArrowEvent, true);

  assertEqualsJSON([KeyCode.DOWN], cvoxDownKey.keys.keyCode);
  assertFalse(cvoxDownKey.stickyMode);
  assertFalse(cvoxDownKey.prefixKey);
  assertTrue(cvoxDownKey.cvoxModifier);

  assertEqualsJSON([false], cvoxDownKey.keys.altGraphKey);
  assertEqualsJSON([false], cvoxDownKey.keys.altKey);
  assertEqualsJSON([false], cvoxDownKey.keys.ctrlKey);
  assertEqualsJSON([false], cvoxDownKey.keys.metaKey);
  assertEqualsJSON([false], cvoxDownKey.keys.searchKeyHeld);
  assertEqualsJSON([false], cvoxDownKey.keys.shiftKey);

  assertEquals(1, cvoxDownKey.length());
});


/**
 * Test equality - Ctrl key vs. Ctrl key with sticky mode on
 * These should be equal because Ctrl should still function even with
 * sticky mode on.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'StickyEquality', function() {
  const ctrlKey = new KeySequence(this.ctrlEvent, false);
  const ctrlSticky = new KeySequence(this.ctrlStickyEvent, false);

  assertTrue(ctrlKey.equals(ctrlSticky));
});


/**
 * Test equality - 'a' key with Shift modifier vs. 'a' key without Shift
 * modifier.
 * These should not be equal because they do not have the same modifiers.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'ShiftEquality', function() {
  const aKey = new KeySequence(this.aEvent, false);
  const shiftA = new KeySequence(this.shiftAEvent, false);

  assertFalse(aKey.equals(shiftA));
});


/**
 * Test equality - 'a' with ChromeVox modifier specified, 'a' with sticky mode
 * on, 'a' with prefix key, and 'a' with ChromeVox modifier held down. These
 * should all be equal to each other.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'FourWayEquality', function() {
  const commandSequence = new KeySequence(this.aEvent, true);
  const stickySequence = new KeySequence(this.aEventSticky, false);
  const prefixSequence = new KeySequence(this.aEventPrefix, false);
  const cvoxModifierSequence = new KeySequence(this.altAEvent);

  assertTrue(commandSequence.equals(stickySequence));
  assertTrue(commandSequence.equals(prefixSequence));
  assertTrue(commandSequence.equals(cvoxModifierSequence));

  assertTrue(stickySequence.equals(commandSequence));
  assertTrue(stickySequence.equals(prefixSequence));
  assertTrue(stickySequence.equals(cvoxModifierSequence));

  assertTrue(prefixSequence.equals(commandSequence));
  assertTrue(prefixSequence.equals(stickySequence));
  assertTrue(prefixSequence.equals(cvoxModifierSequence));

  assertTrue(cvoxModifierSequence.equals(commandSequence));
  assertTrue(cvoxModifierSequence.equals(stickySequence));
  assertTrue(cvoxModifierSequence.equals(prefixSequence));
});


/**
 * Test equality - 'a' key with Shift modifier and prefix vs. 'a' key with Shift
 * modifier and sticky mode vs. 'a' key with Shift modifier and ChromeVox
 * modifier specified vs. 'a' key with ChromeVox modifier held down.
 * These should all be equal to each other..
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'ShiftPrefixEquality', function() {
  const shiftAWithModifier = new KeySequence(this.shiftAEvent, true);
  const shiftAWithPrefix = new KeySequence(this.shiftAPrefixEvent, false);
  const shiftASticky = new KeySequence(this.shiftAStickyEvent, false);
  const cvoxShiftA = new KeySequence(this.altShiftAEvent);

  assertTrue(shiftAWithModifier.equals(shiftAWithPrefix));
  assertTrue(shiftAWithModifier.equals(shiftASticky));
  assertTrue(shiftAWithModifier.equals(cvoxShiftA));

  assertTrue(shiftAWithPrefix.equals(shiftAWithModifier));
  assertTrue(shiftAWithPrefix.equals(shiftASticky));
  assertTrue(shiftAWithPrefix.equals(cvoxShiftA));

  assertTrue(shiftASticky.equals(shiftAWithPrefix));
  assertTrue(shiftASticky.equals(shiftAWithModifier));
  assertTrue(shiftASticky.equals(cvoxShiftA));

  assertTrue(cvoxShiftA.equals(shiftAWithModifier));
  assertTrue(cvoxShiftA.equals(shiftAWithPrefix));
  assertTrue(cvoxShiftA.equals(shiftASticky));
});


/**
 * Test inequality - 'a' with modifier key vs. 'a' without modifier key.
 * These should not be equal.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'Inequality', function() {
  const aNoModifier = new KeySequence(this.aEvent, false);
  const aWithModifier = new KeySequence(this.aEvent, true);

  assertFalse(aNoModifier.equals(aWithModifier));
  assertFalse(aWithModifier.equals(aNoModifier));
});


/**
 * Test equality - adding an additional key onto a sequence.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'CvoxCtrl', function() {
  const cvoxCtrlSequence = new KeySequence(this.ctrlEvent, true);
  assertTrue(cvoxCtrlSequence.addKeyEvent(this.rightArrowEvent));

  assertEquals(2, cvoxCtrlSequence.length());

  // Can't add more than two key events.
  assertFalse(cvoxCtrlSequence.addKeyEvent(this.rightArrowEvent));

  const cvoxCtrlStickySequence = new KeySequence(this.ctrlStickyEvent, false);
  assertTrue(cvoxCtrlStickySequence.addKeyEvent(this.rightArrowEvent));

  const mockCtrlPrefixSequence = new KeySequence(this.ctrlPrefixEvent, false);
  assertTrue(mockCtrlPrefixSequence.addKeyEvent(this.rightArrowEvent));

  assertTrue(cvoxCtrlSequence.equals(cvoxCtrlStickySequence));
  assertTrue(cvoxCtrlStickySequence.equals(cvoxCtrlSequence));

  assertTrue(cvoxCtrlSequence.equals(mockCtrlPrefixSequence));
  assertTrue(mockCtrlPrefixSequence.equals(cvoxCtrlSequence));

  assertTrue(cvoxCtrlStickySequence.equals(mockCtrlPrefixSequence));
  assertTrue(mockCtrlPrefixSequence.equals(cvoxCtrlStickySequence));
});


/**
 * Test for inequality - key sequences in different orders.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'DifferentSequences', function() {
  const cvoxBSequence = new KeySequence(this.bEvent, true);
  assertTrue(cvoxBSequence.addKeyEvent(this.cEvent));

  const cvoxCSequence = new KeySequence(this.cEvent, false);
  assertTrue(cvoxCSequence.addKeyEvent(this.bEvent));

  assertFalse(cvoxBSequence.equals(cvoxCSequence));
  assertFalse(cvoxCSequence.equals(cvoxBSequence));
});


/**
 * Tests modifiers (ctrl, alt, etc) - if two sequences have different modifiers
 * held down then they aren't equal.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'MoreModifiers', function() {
  const ctrlASequence = new KeySequence(this.ctrlAEvent, false);
  const ctrlModifierKeyASequence = new KeySequence(this.ctrlAEvent, true);

  const ctrlBSequence = new KeySequence(this.ctrlBEvent, false);

  const metaASequence = new KeySequence(this.metaAEvent, false);

  assertFalse(ctrlASequence.equals(metaASequence));
  assertFalse(ctrlASequence.equals(ctrlModifierKeyASequence));
  assertFalse(ctrlASequence.equals(ctrlBSequence));
});


/**
 * Tests modifier (ctrl, alt, etc) order - if two sequences have the same
 * modifiers but held down in a different order then they aren't equal.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'ModifierOrder', function() {
  const ctrlShiftSequence = new KeySequence(this.ctrlShiftEvent, false);
  const shiftCtrlSequence = new KeySequence(this.shiftCtrlEvent, true);

  assertFalse(ctrlShiftSequence.equals(shiftCtrlSequence));
});


/**
 * Tests converting from a string to a KeySequence object.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'FromStr', function() {
  const ctrlString = KeySequence.fromStr('Ctrl');
  assertEqualsJSON(ctrlString.keys.ctrlKey, [true]);
  assertEqualsJSON(ctrlString.keys.keyCode, [KeyCode.CONTROL]);

  const modifiedLetterString = KeySequence.fromStr('Ctrl+Z');
  assertEqualsJSON(modifiedLetterString.keys.ctrlKey, [true]);
  assertEqualsJSON(modifiedLetterString.keys.keyCode, [KeyCode.Z]);

  const keyCodeString = KeySequence.fromStr('#9');
  assertEqualsJSON(keyCodeString.keys.keyCode, [KeyCode.TAB]);

  const modifiedKeyCodeString = KeySequence.fromStr('Shift+#9');
  assertEqualsJSON(modifiedKeyCodeString.keys.shiftKey, [true]);
  assertEqualsJSON(modifiedKeyCodeString.keys.keyCode, [KeyCode.TAB]);

  const cvoxLetterString = KeySequence.fromStr('Cvox+U');
  assertTrue(cvoxLetterString.cvoxModifier);
  assertEqualsJSON(cvoxLetterString.keys.keyCode, [KeyCode.U]);

  const cvoxSequenceString = KeySequence.fromStr('Cvox+C>T');
  assertTrue(cvoxSequenceString.cvoxModifier);
  assertEqualsJSON(cvoxSequenceString.keys.keyCode, [KeyCode.C, KeyCode.T]);

  const cvoxSequenceKeyCodeString = KeySequence.fromStr('Cvox+L>#186');
  assertTrue(cvoxSequenceKeyCodeString.cvoxModifier);
  assertEqualsJSON(
      cvoxSequenceKeyCodeString.keys.keyCode, [KeyCode.L, KeyCode.OEM_1]);

  const stickyString = KeySequence.fromStr('Insert>Insert+');
  assertEqualsJSON(stickyString.keys.keyCode, [KeyCode.INSERT, KeyCode.INSERT]);
});


/**
 * Tests converting from a JSON string to a KeySequence object.
 */
AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'Deserialize', function() {
  const forwardSequence = KeySequence.deserialize({
    'cvoxModifier': true,
    'stickyMode': false,
    'prefixKey': false,
    'keys': {
      'ctrlKey': [false],
      'searchKeyHeld': [false],
      'altKey': [false],
      'altGraphKey': [false],
      'shiftKey': [false],
      'metaKey': [false],
      'keyCode': [KeyCode.DOWN],
    },
  });
  assertTrue(forwardSequence.cvoxModifier);
  assertEqualsJSON(forwardSequence.keys.keyCode, [KeyCode.DOWN]);

  const ctrlSequence = KeySequence.deserialize({
    'cvoxModifier': false,
    'stickyMode': true,
    'prefixKey': false,
    'keys': {
      'ctrlKey': [true],
      'searchKeyHeld': [false],
      'altKey': [false],
      'altGraphKey': [false],
      'shiftKey': [false],
      'metaKey': [false],
      'keyCode': [KeyCode.CONTROL],
    },
  });
  assertEqualsJSON(ctrlSequence.keys.ctrlKey, [true]);
  assertEqualsJSON(ctrlSequence.keys.keyCode, [KeyCode.CONTROL]);
});

AX_TEST_F(
    'ChromeVoxKeySequenceUnitTest', 'DeserializeAltShiftCvoxMod', function() {
      KeySequence.modKeyStr = 'Alt+Shift';

      // Build a key sequence that does not strip modifiers when deserializing.
      // This feature is important for sequences that contain part or all of the
      // modifiers in the cvox modifier as specified in the key map. This
      // happens by default when deserializing.
      //
      // For example, at runtime, a user presses Shift+H with sticky mode on.
      // This should match against a key sequence that has cvoxModifier set
      // along with shift key set.
      const prevHeadingUnstripped = {'shiftKey': [true], keyCode: [KeyCode.H]};
      const prevHeadingSeq = KeySequence.deserialize(
          {'cvoxModifier': true, keys: prevHeadingUnstripped});

      assertTrue(prevHeadingSeq.cvoxModifier);
      assertTrue(prevHeadingSeq.keys.shiftKey[0]);
    });

AX_TEST_F(
    'ChromeVoxKeySequenceUnitTest', 'DeserializeSearchCvoxMod', function() {
      // Test the case when we do want to strip modifiers when deserializing.
      // This is important when the key sequence in the key map and the key
      // sequence at runtime both contain the bare cvox modifier as a key code
      // such as in the case of the Search sticky key and Search cvox modifier.
      // Stripping happens by default for key events at runtime.
      KeySequence.modKeyStr = 'Search';

      // First, assert that unstripped seqs imply various modifier fields get
      // set.
      let stickySeq =
          KeySequence.deserialize({keys: {keyCode: [KeyCode.SEARCH]}});
      assertTrue(stickySeq.cvoxModifier);
      assertTrue(stickySeq.keys.metaKey[0]);
      assertTrue(stickySeq.keys.searchKeyHeld[0]);

      // Next, assert that stripping causes those modifiers to get unset. This
      // is desirable at runtime so that we can match against the stripped
      // runtime key seqs.
      stickySeq = KeySequence.deserialize(
          {'skipStripping': false, keys: {keyCode: [KeyCode.SEARCH]}});
      assertTrue(stickySeq.cvoxModifier);
      assertFalse(stickySeq.keys.metaKey[0]);
      assertFalse(stickySeq.keys.searchKeyHeld[0]);
    });

AX_TEST_F('ChromeVoxKeySequenceUnitTest', 'RequireStickyMode', function() {
  const oneFromMap = KeySequence.deserialize(
      {requireStickyMode: true, keys: {keyCode: [KeyCode.ONE]}});

  assertFalse(oneFromMap.cvoxModifier);
  assertTrue(oneFromMap.requireStickyMode);
  assertFalse(oneFromMap.stickyMode);

  // Pressing one doesn't trigger the key because it requires sticky mode.
  const oneFromEvt = KeySequence.deserialize({keys: {keyCode: [KeyCode.ONE]}});
  assertFalse(oneFromMap.equals(oneFromEvt));

  // Even modified, it should not match.
  const modOneFromEvt = KeySequence.deserialize(
      {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE]}});
  assertFalse(oneFromMap.equals(modOneFromEvt));

  // But, with sticky mode on in the event, it should match.
  const stickyOneFromEvt = KeySequence.deserialize(
      {stickyMode: true, keys: {keyCode: [KeyCode.ONE]}});
  assertTrue(stickyOneFromEvt.equals(oneFromMap));

  // Finally, with both modifier and sticky on, it doesn't match.
  const stickyModOneFromEvt = KeySequence.deserialize(
      {stickyMode: true, cvoxModifier: true, keys: {keyCode: [KeyCode.ONE]}});
  assertFalse(stickyModOneFromEvt.equals(oneFromMap));
});
