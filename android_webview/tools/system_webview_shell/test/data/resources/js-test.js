// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

debug = function debug(msg)
{
    console.log(msg);
};

description = function description(msg, quiet)
{
    console.log(msg);
};

finishJSTest = function finishJSTest() {
    console.log("TEST FINISHED");
};

function handleTestFinished() {
    if (!window.jsTestIsAsync)
        finishJSTest();
}

function testFailed(msg) {
    debug('FAIL: ' + msg);
}

function testPassed(msg)
{
    debug('PASS: ' + msg);
}

function isWorker()
{
    // It's conceivable that someone would stub out 'document' in a worker so
    // also check for childNodes, an arbitrary DOM-related object that is
    // meaningless in a WorkerContext.
    return (typeof document === 'undefined' ||
            typeof document.childNodes === 'undefined') && !!self.importScripts;
}

if (!isWorker()) {
    window.addEventListener('DOMContentLoaded', handleTestFinished, false);
}

function _compareLessThan(_a, _b) {
  return _a < _b;
}

function _compareGreaterThan(_a, _b) {
  return _a > _b;
}

// TODO(timvolodine): consider moving this code to js-test.js in blink and
// reusing it from there (crbug.com/535209).
function _comp(_a, _b, _inv_comparison_func, _comparison_str) {
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: _comp expects string arguments");

  var _exception;
  var _av;
  try {
    _av = eval(_a);
  } catch (e) {
    _exception = e;
  }
  var _bv = eval(_b);

  if (_exception) {
    testFailed(_a + " should be" + _comparison_str + _b + ". Threw exception "
        + _exception);
  } else if (typeof _av == "undefined" || _inv_comparison_func(_av, _bv)) {
    testFailed(_a + " should be" + _comparison_str + _b + ". Was " + _av
        + " (of type " + typeof _av + ").");
  } else {
    testPassed(_a + " is" + _comparison_str + _b);
  }
}

function shouldBeGreaterThanOrEqual(_a, _b) {
  _comp(_a, _b, _compareLessThan, " >= ");
}

function shouldBeLessThanOrEqual(_a, _b) {
  _comp(_a, _b, _compareGreaterThan, " <= ");
}

// Functions in common with js-test.js in blink,
// see third_party/WebKit/LayoutTests/resources/js-test.js

function areArraysEqual(a, b)
{
    try {
        if (a.length !== b.length)
            return false;
        for (var i = 0; i < a.length; i++)
            if (a[i] !== b[i])
                return false;
    } catch (ex) {
        return false;
    }
    return true;
}

// Returns a sorted array of property names of object.  This function returns
// not only own properties but also properties on prototype chains.
function getAllPropertyNames(object) {
    var properties = [];
    for (var property in object) {
        properties.push(property);
    }
    return properties.sort();
}

function isNewSVGTearOffType(v)
{
    return ['[object SVGLength]', '[object SVGLengthList]',
            '[object SVGPoint]', '[object SVGPointList]',
            '[object SVGNumber]', '[object SVGTransform]',
            '[object SVGTransformList]'].indexOf(""+v) != -1;
}

function stringify(v)
{
    if (isNewSVGTearOffType(v))
        return v.valueAsString;
    if (v === 0 && 1/v < 0)
        return "-0";
    else return "" + v;
}

function isResultCorrect(actual, expected)
{
    if (expected === 0)
        return actual === expected && (1/actual) === (1/expected);
    if (actual === expected)
        return true;
    // http://crbug.com/308818 : The new implementation of SVGListProperties
    // do not necessary return the same wrapper object, so === operator would
    // not work. We compare for their string representation instead.
    if (isNewSVGTearOffType(expected) && typeof(expected) == typeof(actual)
            && actual.valueAsString == expected.valueAsString)
        return true;
    if (typeof(expected) == "number" && isNaN(expected))
        return typeof(actual) == "number" && isNaN(actual);
    if (expected && (Object.prototype.toString.call(expected)
                     == Object.prototype.toString.call([])))
        return areArraysEqual(actual, expected);
    return false;
}

function shouldBe(_a, _b, quiet, opt_tolerance)
{
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBe() expects string arguments");
    var _exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        _exception = e;
    }
    var _bv = eval(_b);

    if (_exception)
        testFailed(_a + " should be " + _bv + ". Threw exception "
                + _exception);
    else if (isResultCorrect(_av, _bv)
            || (typeof opt_tolerance == 'number' && typeof _av == 'number'
                && Math.abs(_av - _bv) <= opt_tolerance)) {
        if (!quiet) {
            testPassed(_a + " is " + _b);
        }
    } else if (typeof(_av) == typeof(_bv)) {
        testFailed(_a + " should be " + _bv + ". Was " + stringify(_av) + ".");
    } else {
        testFailed(_a + " should be " + _bv + " (of type " + typeof _bv
                + "). Was " + _av + " (of type " + typeof _av + ").");
    }
}

function shouldBeEqualToString(a, b)
{
    if (typeof a !== "string" || typeof b !== "string")
        debug("WARN: shouldBeEqualToString() expects string arguments");
    var unevaledString = JSON.stringify(b);
    shouldBe(a, unevaledString);
}

function shouldBeTrue(a, quiet) { shouldBe(a, "true", quiet); }
function shouldBeFalse(a, quiet) { shouldBe(a, "false", quiet); }
