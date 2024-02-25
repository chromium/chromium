// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

UnserializableSpan = function() {};

StatelessSerializableSpan = function() {};

NonStatelessSerializableSpan = function(value) {
  this.value = value;
};

/**
 * @param {!Object} obj object containing the
 *     serializable representation.
 * @return {!Object} The Spannable.
 */
NonStatelessSerializableSpan.fromJson = function(obj) {
  return new NonStatelessSerializableSpan(obj.value / 2);
};

/**
 * @return {Object} the json serializable form.
 */
NonStatelessSerializableSpan.prototype.toJson = function() {
  return {value: this.value * 2};
};

/**
 * @param {Spannable} spannable
 * @param {*} annotation
 */
function assertSpanNotFound(spannable, annotation) {
  assertFalse(spannable.hasSpan(annotation));
  assertException(
      'Span ' + annotation + ' shouldn\'t be in spannable ' + spannable,
      function() {
        spannable.getSpanStart(annotation);
      },
      'Error');
  assertException(
      'Span ' + annotation + ' shouldn\'t be in spannable ' + spannable,
      function() {
        spannable.getSpanEnd(annotation);
      },
      'Error');
  assertException(
      'Span ' + annotation + ' shouldn\'t be in spannable ' + spannable,
      function() {
        spannable.getSpanLength(annotation);
      },
      'Error');
}

/**
 * Test fixture.
 */
ChromeVoxSpannableUnitTest = class extends ChromeVoxE2ETest {
  /** @override */
  setUp() {
    super.setUp();
  }

  async setUpDeferred() {
    await super.setUpDeferred();

    Spannable.registerStatelessSerializableSpan(
        StatelessSerializableSpan, 'StatelessSerializableSpan');

    Spannable.registerSerializableSpan(
        NonStatelessSerializableSpan, 'NonStatelessSerializableSpan',
        NonStatelessSerializableSpan.fromJson,
        NonStatelessSerializableSpan.prototype.toJson);
  }
};

AX_TEST_F('ChromeVoxSpannableUnitTest', 'ToStringUnannotated', function() {
  assertEquals('', new Spannable().toString());
  assertEquals('hello world', new Spannable('hello world').toString());
});

/** Tests that toString works correctly on annotated strings. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'ToStringAnnotated', function() {
  const spannable = new Spannable('Hello Google');
  spannable.setSpan('http://www.google.com/', 6, 12);
  assertEquals('Hello Google', spannable.toString());
});

/** Tests the length calculation. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'LengthProperty', function() {
  const spannable = new Spannable('Hello');
  spannable.setSpan({}, 0, 3);
  assertEquals(5, spannable.length);
  spannable.append(' world');
  assertEquals(11, spannable.length);
  spannable.append(new Spannable(' from Spannable'));
  assertEquals(26, spannable.length);
});

/** Tests that a span can be added and retrieved at the beginning. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SpanBeginning', function() {
  const annotation = {};
  const spannable = new Spannable('Hello world');
  spannable.setSpan(annotation, 0, 5);
  assertTrue(spannable.hasSpan(annotation));
  assertSame(annotation, spannable.getSpan(0));
  assertSame(annotation, spannable.getSpan(3));
  assertUndefined(spannable.getSpan(5));
  assertUndefined(spannable.getSpan(8));
});

/** Tests that a span can be added and retrieved at the beginning. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SpanEnd', function() {
  const annotation = {};
  const spannable = new Spannable('Hello world');
  spannable.setSpan(annotation, 6, 11);
  assertTrue(spannable.hasSpan(annotation));
  assertUndefined(spannable.getSpan(3));
  assertUndefined(spannable.getSpan(5));
  assertSame(annotation, spannable.getSpan(6));
  assertSame(annotation, spannable.getSpan(10));
});

/** Tests that a zero-length span is not retrieved. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SpanZeroLength', function() {
  const annotation = {};
  const spannable = new Spannable('Hello world');
  spannable.setSpan(annotation, 3, 3);
  assertTrue(spannable.hasSpan(annotation));
  assertUndefined(spannable.getSpan(2));
  assertUndefined(spannable.getSpan(3));
  assertUndefined(spannable.getSpan(4));
});

/** Tests that a removed span is not returned. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'RemoveSpan', function() {
  const annotation = {};
  const spannable = new Spannable('Hello world');
  spannable.setSpan(annotation, 0, 3);
  assertSame(annotation, spannable.getSpan(1));
  spannable.removeSpan(annotation);
  assertFalse(spannable.hasSpan(annotation));
  assertUndefined(spannable.getSpan(1));
});

/** Tests that adding a span in one place removes it from another. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SetSpanMoves', function() {
  const annotation = {};
  const spannable = new Spannable('Hello world');
  spannable.setSpan(annotation, 0, 3);
  assertSame(annotation, spannable.getSpan(1));
  assertUndefined(spannable.getSpan(4));
  spannable.setSpan(annotation, 3, 6);
  assertUndefined(spannable.getSpan(1));
  assertSame(annotation, spannable.getSpan(4));
});

/** Tests that setSpan objects to out-of-range arguments. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SetSpanRangeError', function() {
  const spannable = new Spannable('Hello world');

  // Start index out of range.
  assertException('expected range error', function() {
    spannable.setSpan({}, -1, 0);
  }, 'RangeError');

  // End index out of range.
  assertException('expected range error', function() {
    spannable.setSpan({}, 0, 12);
  }, 'RangeError');

  // End before start.
  assertException('expected range error', function() {
    spannable.setSpan({}, 1, 0);
  }, 'RangeError');
});

/**
 * Tests that multiple spans can be retrieved at one point.
 * The first one added which applies should be returned by getSpan.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'MultipleSpans', function() {
  const annotation1 = {number: 1};
  const annotation2 = {number: 2};
  assertNotSame(annotation1, annotation2);
  const spannable = new Spannable('Hello world');
  spannable.setSpan(annotation1, 1, 4);
  spannable.setSpan(annotation2, 2, 7);
  assertSame(annotation1, spannable.getSpan(1));
  assertDeepEquals([annotation1], spannable.getSpans(1));
  assertSame(annotation1, spannable.getSpan(3));
  assertDeepEquals([annotation1, annotation2], spannable.getSpans(3));
  assertSame(annotation2, spannable.getSpan(6));
  assertDeepEquals([annotation2], spannable.getSpans(6));
});

/** Tests that appending appends the strings. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'AppendToString', function() {
  const spannable = new Spannable('Google');
  assertEquals('Google', spannable.toString());
  spannable.append(' Chrome');
  assertEquals('Google Chrome', spannable.toString());
  spannable.append(new Spannable('Vox'));
  assertEquals('Google ChromeVox', spannable.toString());
});

/**
 * Tests that appending Spannables combines annotations.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'AppendAnnotations', function() {
  const annotation1 = {number: 1};
  const annotation2 = {number: 2};
  assertNotSame(annotation1, annotation2);
  const left = new Spannable('hello');
  left.setSpan(annotation1, 0, 3);
  const right = new Spannable(' world');
  right.setSpan(annotation2, 0, 3);
  left.append(right);
  assertSame(annotation1, left.getSpan(1));
  assertSame(annotation2, left.getSpan(6));
});

/**
 * Tests that a span's bounds can be retrieved.
 */
AX_TEST_F(
    'ChromeVoxSpannableUnitTest', 'GetSpanStartAndEndAndLength', function() {
      const annotation = {};
      const spannable = new Spannable('potato wedges');
      spannable.setSpan(annotation, 8, 12);
      assertEquals(8, spannable.getSpanStart(annotation));
      assertEquals(12, spannable.getSpanEnd(annotation));
      assertEquals(4, spannable.getSpanLength(annotation));
    });

/**
 * Tests that an absent span's bounds are reported correctly.
 */
AX_TEST_F(
    'ChromeVoxSpannableUnitTest', 'GetSpanStartAndEndAndLengthAbsent',
    function() {
      const annotation = {};
      const spannable = new Spannable('potato wedges');
      assertSpanNotFound(spannable, annotation);
    });

/**
 * Test that a zero length span can still be found.
 */
AX_TEST_F(
    'ChromeVoxSpannableUnitTest', 'GetSpanStartAndEndAndLengthZeroLength',
    function() {
      const annotation = {};
      const spannable = new Spannable('potato wedges');
      spannable.setSpan(annotation, 8, 8);
      assertEquals(8, spannable.getSpanStart(annotation));
      assertEquals(8, spannable.getSpanEnd(annotation));
      assertEquals(0, spannable.getSpanLength(annotation));
    });

/**
 * Tests that == (but not ===) objects are treated distinctly when getting
 * span bounds.
 */
AX_TEST_F(
    'ChromeVoxSpannableUnitTest', 'GetSpanStartAndEndEquality', function() {
      // Note that 0 == '' and '' == 0 in JavaScript.
      const spannable = new Spannable('wat');
      spannable.setSpan(0, 0, 0);
      spannable.setSpan('', 1, 3);
      assertEquals(0, spannable.getSpanStart(0));
      assertEquals(0, spannable.getSpanEnd(0));
      assertEquals(1, spannable.getSpanStart(''));
      assertEquals(3, spannable.getSpanEnd(''));
    });

/**
 * Tests that substrings have the correct character sequence.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'Substring', function() {
  const assertSubstringResult = function(expected, initial, start, opt_end) {
    const spannable = new Spannable(initial);
    const substring = spannable.substring(start, opt_end);
    assertEquals(expected, substring.toString());
  };
  assertSubstringResult('Page', 'Google PageRank', 7, 11);
  assertSubstringResult('Goog', 'Google PageRank', 0, 4);
  assertSubstringResult('Rank', 'Google PageRank', 11, 15);
  assertSubstringResult('Rank', 'Google PageRank', 11);
});

/**
 * Tests that substring arguments are validated properly.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SubstringRangeError', function() {
  const assertRangeError = function(initial, start, opt_end) {
    const spannable = new Spannable(initial);
    assertException('expected range error', function() {
      spannable.substring(start, opt_end);
    }, 'RangeError');
  };
  assertRangeError('Google PageRank', -1, 5);
  assertRangeError('Google PageRank', 0, 99);
  assertRangeError('Google PageRank', 5, 2);
});

/**
 * Tests that spans in the substring range are preserved.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SubstringSpansIncluded', function() {
  const assertSpanIncluded = function(
      expectedSpanStart, expectedSpanEnd, initial, initialSpanStart,
      initialSpanEnd, start, opt_end) {
    const annotation = {};
    const spannable = new Spannable(initial);
    spannable.setSpan(annotation, initialSpanStart, initialSpanEnd);
    const substring = spannable.substring(start, opt_end);
    assertTrue(substring.hasSpan(annotation));
    assertEquals(expectedSpanStart, substring.getSpanStart(annotation));
    assertEquals(expectedSpanEnd, substring.getSpanEnd(annotation));
  };
  assertSpanIncluded(1, 5, 'potato wedges', 8, 12, 7);
  assertSpanIncluded(1, 5, 'potato wedges', 8, 12, 7, 13);
  assertSpanIncluded(1, 5, 'potato wedges', 8, 12, 7, 12);
  assertSpanIncluded(0, 4, 'potato wedges', 8, 12, 8, 12);
  assertSpanIncluded(0, 3, 'potato wedges', 0, 3, 0);
  assertSpanIncluded(0, 3, 'potato wedges', 0, 3, 0, 3);
  assertSpanIncluded(0, 3, 'potato wedges', 0, 3, 0, 6);
  assertSpanIncluded(0, 5, 'potato wedges', 8, 13, 8);
  assertSpanIncluded(0, 5, 'potato wedges', 8, 13, 8, 13);
  assertSpanIncluded(1, 6, 'potato wedges', 8, 13, 7, 13);

  // Note: we should keep zero-length spans, even at the edges of the range.
  assertSpanIncluded(0, 0, 'potato wedges', 0, 0, 0, 0);
  assertSpanIncluded(0, 0, 'potato wedges', 0, 0, 0, 6);
  assertSpanIncluded(1, 1, 'potato wedges', 8, 8, 7, 13);
  assertSpanIncluded(6, 6, 'potato wedges', 6, 6, 0, 6);
});

/**
 * Tests that spans outside the range are omitted.
 * It's fine to keep zero-length spans at the ends, though.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SubstringSpansExcluded', function() {
  const assertSpanExcluded = function(
      initial, spanStart, spanEnd, start, opt_end) {
    const annotation = {};
    const spannable = new Spannable(initial);
    spannable.setSpan(annotation, spanStart, spanEnd);
    const substring = spannable.substring(start, opt_end);
    assertSpanNotFound(substring, annotation);
  };
  assertSpanExcluded('potato wedges', 8, 12, 0, 6);
  assertSpanExcluded('potato wedges', 7, 12, 0, 6);
  assertSpanExcluded('potato wedges', 0, 6, 8);
  assertSpanExcluded('potato wedges', 6, 6, 8);
});

/**
 * Tests that spans which cross the boundary are clipped.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'SubstringSpansClipped', function() {
  const assertSpanIncluded = function(
      expectedSpanStart, expectedSpanEnd, initial, initialSpanStart,
      initialSpanEnd, start, opt_end) {
    const annotation = {};
    const spannable = new Spannable(initial);
    spannable.setSpan(annotation, initialSpanStart, initialSpanEnd);
    const substring = spannable.substring(start, opt_end);
    assertEquals(expectedSpanStart, substring.getSpanStart(annotation));
    assertEquals(expectedSpanEnd, substring.getSpanEnd(annotation));
  };
  assertSpanIncluded(0, 4, 'potato wedges', 7, 13, 8, 12);
  assertSpanIncluded(0, 0, 'potato wedges', 0, 6, 0, 0);
  assertSpanIncluded(0, 0, 'potato wedges', 0, 6, 6, 6);

  // The first of the above should produce "edge".
  assertEquals(
      'edge', new Spannable('potato wedges').substring(8, 12).toString());
});

/**
 * Tests that whitespace is trimmed.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'Trim', function() {
  const assertTrimResult = function(expected, initial) {
    assertEquals(expected, new Spannable(initial).trim().toString());
  };
  assertTrimResult('John F. Kennedy', 'John F. Kennedy');
  assertTrimResult('John F. Kennedy', '  John F. Kennedy');
  assertTrimResult('John F. Kennedy', 'John F. Kennedy     ');
  assertTrimResult('John F. Kennedy', '   \r\t   \nJohn F. Kennedy\n\n \n');
  assertTrimResult('', '');
  assertTrimResult('', '     \t\t    \n\r');
});

/**
 * Tests that trim keeps, drops and clips spans.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'TrimSpans', function() {
  const spannable = new Spannable(' \t Kennedy\n');
  spannable.setSpan('tab', 1, 2);
  spannable.setSpan('jfk', 3, 10);
  spannable.setSpan('jfk-newline', 3, 11);
  const trimmed = spannable.trim();
  assertSpanNotFound(trimmed, 'tab');
  assertEquals(0, trimmed.getSpanStart('jfk'));
  assertEquals(7, trimmed.getSpanEnd('jfk'));
  assertEquals(0, trimmed.getSpanStart('jfk-newline'));
  assertEquals(7, trimmed.getSpanEnd('jfk-newline'));
});

/**
 * Tests that when a string is all whitespace, we trim off the *end*.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'TrimAllWhitespace', function() {
  const spannable = new Spannable('    ');
  spannable.setSpan('cursor 1', 0, 0);
  spannable.setSpan('cursor 2', 2, 2);
  const trimmed = spannable.trim();
  assertEquals(0, trimmed.getSpanStart('cursor 1'));
  assertEquals(0, trimmed.getSpanEnd('cursor 1'));
  assertSpanNotFound(trimmed, 'cursor 2');
});

/**
 * Tests finding a span which is an instance of a given class.
 */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'GetSpanInstanceOf', function() {
  function ExampleConstructorBase() {}
  function ExampleConstructor1() {}
  function ExampleConstructor2() {}
  function ExampleConstructor3() {}
  ExampleConstructor1.prototype = new ExampleConstructorBase();
  ExampleConstructor2.prototype = new ExampleConstructorBase();
  ExampleConstructor3.prototype = new ExampleConstructorBase();
  const ex1 = new ExampleConstructor1();
  const ex2 = new ExampleConstructor2();
  const spannable = new Spannable('Hello world');
  spannable.setSpan(ex1, 0, 0);
  spannable.setSpan(ex2, 1, 1);
  assertEquals(ex1, spannable.getSpanInstanceOf(ExampleConstructor1));
  assertEquals(ex2, spannable.getSpanInstanceOf(ExampleConstructor2));
  assertUndefined(spannable.getSpanInstanceOf(ExampleConstructor3));
  assertEquals(ex1, spannable.getSpanInstanceOf(ExampleConstructorBase));
});

/** Tests trimming only left or right. */
AX_TEST_F('ChromeVoxSpannableUnitTest', 'TrimLeftOrRight', function() {
  const spannable = new Spannable('    ');
  spannable.setSpan('cursor 1', 0, 0);
  spannable.setSpan('cursor 2', 2, 2);
  const trimmed = spannable.trimLeft();
  assertEquals(0, trimmed.getSpanStart('cursor 1'));
  assertEquals(0, trimmed.getSpanEnd('cursor 1'));
  assertSpanNotFound(trimmed, 'cursor 2');

  const spannable2 = new Spannable('0  ');
  spannable2.setSpan('cursor 1', 0, 0);
  spannable2.setSpan('cursor 2', 2, 2);
  let trimmed2 = spannable2.trimLeft();
  assertEquals(0, trimmed2.getSpanStart('cursor 1'));
  assertEquals(0, trimmed2.getSpanEnd('cursor 1'));
  assertEquals(2, trimmed2.getSpanStart('cursor 2'));
  assertEquals(2, trimmed2.getSpanEnd('cursor 2'));
  trimmed2 = trimmed2.trimRight();
  assertEquals(0, trimmed2.getSpanStart('cursor 1'));
  assertEquals(0, trimmed2.getSpanEnd('cursor 1'));
  assertSpanNotFound(trimmed2, 'cursor 2');

  const spannable3 = new Spannable('  0');
  spannable3.setSpan('cursor 1', 0, 0);
  spannable3.setSpan('cursor 2', 2, 2);
  let trimmed3 = spannable3.trimRight();
  assertEquals(0, trimmed3.getSpanStart('cursor 1'));
  assertEquals(0, trimmed3.getSpanEnd('cursor 1'));
  assertEquals(2, trimmed3.getSpanStart('cursor 2'));
  assertEquals(2, trimmed3.getSpanEnd('cursor 2'));
  trimmed3 = trimmed3.trimLeft();
  assertSpanNotFound(trimmed3, 'cursor 1');
  assertEquals(0, trimmed3.getSpanStart('cursor 2'));
  assertEquals(0, trimmed3.getSpanEnd('cursor 2'));
});

AX_TEST_F('ChromeVoxSpannableUnitTest', 'Serialize', function() {
  const fresh = new Spannable('text');
  const freshStatelessSerializable = new StatelessSerializableSpan();
  const freshNonStatelessSerializable = new NonStatelessSerializableSpan(14);
  fresh.setSpan(new UnserializableSpan(), 0, 1);
  fresh.setSpan(freshStatelessSerializable, 0, 2);
  fresh.setSpan(freshNonStatelessSerializable, 3, 4);
  const thawn = Spannable.fromJson(fresh.toJson());
  const thawnStatelessSerializable =
      thawn.getSpanInstanceOf(StatelessSerializableSpan);
  const thawnNonStatelessSerializable =
      thawn.getSpanInstanceOf(NonStatelessSerializableSpan);
  assertEquals('text', thawn.toString());
  assertUndefined(thawn.getSpanInstanceOf(UnserializableSpan));
  assertDeepEquals(
      fresh.getSpanStart(freshStatelessSerializable),
      thawn.getSpanStart(thawnStatelessSerializable));
  assertDeepEquals(
      fresh.getSpanEnd(freshStatelessSerializable),
      thawn.getSpanEnd(thawnStatelessSerializable));
  assertDeepEquals(
      freshNonStatelessSerializable, thawnNonStatelessSerializable);
});

AX_TEST_F('ChromeVoxSpannableUnitTest', 'GetSpanIntervals', function() {
  function Foo() {}
  function Bar() {}
  const ms = new MultiSpannable('f12b45f78b01');
  const foo = new Foo();
  const bar = new Bar();
  ms.setSpan(foo, 0, 3);
  ms.setSpan(bar, 3, 6);
  ms.setSpan(foo, 6, 9);
  ms.setSpan(bar, 9, 12);
  assertEquals(2, ms.getSpansInstanceOf(Foo).length);
  assertEquals(2, ms.getSpansInstanceOf(Bar).length);

  const fooIntervals = ms.getSpanIntervals(foo);
  assertEquals(2, fooIntervals.length);
  assertEquals(0, fooIntervals[0].start);
  assertEquals(3, fooIntervals[0].end);
  assertEquals(6, fooIntervals[1].start);
  assertEquals(9, fooIntervals[1].end);

  const barIntervals = ms.getSpanIntervals(bar);
  assertEquals(2, barIntervals.length);
  assertEquals(3, barIntervals[0].start);
  assertEquals(6, barIntervals[0].end);
  assertEquals(9, barIntervals[1].start);
  assertEquals(12, barIntervals[1].end);
});
