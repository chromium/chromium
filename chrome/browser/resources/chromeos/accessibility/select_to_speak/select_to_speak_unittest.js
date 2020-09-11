// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for select_to_speak.js.
 */
SelectToSpeakUnitTest = class extends testing.Test {};

/** @override */
SelectToSpeakUnitTest.prototype.extraLibraries = [
  '../common/closure_shim.js', '../common/key_code.js', 'test_support.js',
  'select_to_speak.js'
];


TEST_F('SelectToSpeakUnitTest', 'getGSuiteAppRoot', function() {
  const root = {url: 'https://docs.google.com/presentation/p/cats_r_awesome'};
  const div1 = {root};
  const frame1 = {url: 'about:blank', parent: div1};
  const div2 = {root: frame1};
  const frame2 = {url: 'about:blank', parent: div2};
  const focus = {root: frame2};
  assertEquals(getGSuiteAppRoot(focus), root);
  assertEquals(getGSuiteAppRoot(div2), root);

  // Sandbox URLs should still work.
  root.url = 'https://docs.sandbox.google.com/spreadsheets/s/spreadsheet';
  assertEquals(getGSuiteAppRoot(focus), root);

  // GSuite app embedded in something else
  const parent = {url: 'https://www.has_embedded_doc.com'};
  const div3 = {root: parent};
  root.parent = div3;
  assertEquals(getGSuiteAppRoot(focus), root);
  assertEquals(getGSuiteAppRoot(div2), root);

  // Not in GSuite app
  root.url = 'https://www.not_a_doc.com';
  assertEquals(getGSuiteAppRoot(focus), null);
  assertEquals(getGSuiteAppRoot(div2), null);
});
