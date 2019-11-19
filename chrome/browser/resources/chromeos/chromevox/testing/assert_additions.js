// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that a given argument's value is undefined.
 * @param {object} a The argument to check.
 */
function assertUndefined(a) {
  if (a !== undefined) {
    throw new Error('Assertion failed: expected undefined');
  }
}

/**
 * Asserts that the argument is neither null nor undefined.
 * @param {object} obj The argument to check.
 * @param {string=} opt_message Error message if the condition is not met.
 */
function assertNotNullNorUndefined(obj, opt_message) {
  if (obj === undefined || obj === null) {
    throw new Error(
        'Can\'t be null or undefined: ' + (opt_message || '') + '\n' +
        'Actual: ' + obj);
  }
}

/**
 * Asserts that a given function call throws an exception.
 * @param {string} msg Message to print if exception not thrown.
 * @param {Function} fn The function to call.
 * @param {string} error The name of the exception we expect {@code fn} to
 *     throw.
 */
function assertException(msg, fn, error) {
  try {
    fn();
  } catch (e) {
    if (error && e.name != error) {
      throw new Error(
          'Expected to throw ' + error + ' but threw ' + e.name + ' - ' + msg);
    }
    return;
  }

  throw new Error('Expected to throw exception ' + error + ' - ' + msg);
}

/**
 * Asserts that two arrays of strings are equal.
 * @param {Array<string>} array1 The expected array.
 * @param {Array<string>} array2 The test array.
 */
function assertEqualStringArrays(array1, array2) {
  var same = true;
  if (array1.length != array2.length) {
    same = false;
  }
  for (var i = 0; i < Math.min(array1.length, array2.length); i++) {
    if (array1[i].trim() != array2[i].trim()) {
      same = false;
    }
  }
  if (!same) {
    throw new Error(
        'Expected ' + JSON.stringify(array1) + ', got ' +
        JSON.stringify(array2));
  }
}

/**
 * Asserts that two objects have the same JSON serialization.
 * @param {Object} expected The expected object.
 * @param {Object} actual The actual object.
 * @param {string=} opt_message Message used for errors.
 */
function assertEqualsJSON(expected, actual, opt_message) {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(
        (opt_message ? opt_message + '\n' : '') + 'Expected ' +
        JSON.stringify(expected) + '\n' +
        'Got      ' + JSON.stringify(actual));
  }
}

/**
 * Asserts that two ArrayBuffers have the same content.
 * @param {ArrayBuffer} arrayBufA The expected ArrayBuffer.
 * @param {ArrayBuffer} arrayBufB The test ArrayBuffer.
 */
function assertArrayBuffersEquals(arrayBufA, arrayBufB) {
  var view1 = new Uint8Array(arrayBufA);
  var view2 = new Uint8Array(arrayBufB);
  assertEquals(JSON.stringify(view1), JSON.stringify(view2));
}

/**
 * Asserts that two Arrays have the same content.
 * @param {ArrayBuffer} arrayA The expected array.
 * @param {ArrayBuffer} arrayB The test array.
 */
function assertArraysEquals(arrayA, arrayB) {
  assertEquals(JSON.stringify(arrayA), JSON.stringify(arrayB));
}

/**
 * Asserts and fails immediately once called.
 * @param {string=} opt_msg
 */
function assertNotReached(opt_msg) {
  assertFalse(true, opt_msg);
}

/**
 * Asserts an actual DOM equals an expected stringified DOM.
 * @param {string} expected
 * @param {Node} actual
 */
function assertEqualsDOM(expected, actual) {
  expected = expected.replace(/>\s+</gm, '><').trim(/\s/gm);
  var actualStr = actual.outerHTML;
  actualStr = actualStr.replace(/>\s+</gm, '><').trim(/\s/gm);

  for (var i = 0; i < expected.length; i++)
    assertEquals(
        expected[i], actualStr[i],
        'Mismatch at index ' + i + ' in expected:\n' + expected +
            '\nactual:\n' + actualStr + '\n');
}

assertSame = assertEquals;
assertNotSame = assertNotEquals;
