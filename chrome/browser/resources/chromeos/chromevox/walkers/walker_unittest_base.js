// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../testing/chromevox_unittest_base.js']);

/**
 * Base class for walker test fixtures.
 * @constructor
 * @extends {ChromeVoxUnitTestBase}
 */
function ChromeVoxWalkerUnitTestBase() {}

ChromeVoxWalkerUnitTestBase.prototype = {
  __proto__: ChromeVoxUnitTestBase.prototype,

  /** @override */
  closureModuleDeps: ['TestMsgs', 'cvox.CursorSelection'],

  /**
   * Common set up for all walker test cases.
   */
  setUp: function() {
    // Needed for getDescription and getGranularityMsg.
    Msgs = TestMsgs;

    // Delete all nodes in the body.
    while (document.body.hasChildNodes()) {
      document.body.removeChild(document.body.lastChild);
    }

    this.walker = this.newWalker();
  },

  /**
   * Returns a new walker appropriate for the child test.
   * @return {!cvox.AbstractWalker} The walker instance.
   */
  // Closure library is not available when this literal is evaluated, so
  // we can't use goog.abstractMethod here.
  newWalker: function() {
    throw Error('newWalker not implemented.');
  },

  /**
   * Makes testing much less verbose. Executes the command on the
   * selection, then asserts that for all the parameters passed in desc,
   * the new selection matches. Returns the new selections if assertion passes.
   * NOTE: If you change the parameters here, you should also change the
   * whitelist above.
   * @param {!cvox.CursorSelection} sel The selection.
   * @param {!string|!cvox.CursorSelection} opt_cmdOrDest The command to
   *  execute, or the override returned selection.
   * @param {{selText: string=,
   *          selNodeId: string=,
   *          selParentNodeId: string=,
   *          selStartIndex: number=,
   *          selEndIndex: number=,
   *          selReversed: boolean=,
   *          descText: string=,
   *          descContext: string=,
   *          descAnnotation: string=,
   *          descUserValue: string=,
   *          descPersonality: string=}} desc The parameters to assert.
   *    selText: The text in the new selection matches for both start and end.
   *    selNodeId: The node in the new selection matches for both start and end.
   *    selParentNodeId: The parent node of both start and end matches.
   *    selStartIndex: The index of the absolute start.
   *    selEndIndex: The index of the absolute end.
   *    selReversed: True if should be reversed.
   *    descText: The text in the NavDescription when getDescription is called.
   *    descContext: The context in the NavDescription
   *    descAnnotation: The annotation in the NavDescription
   *    descUserValue: The userValue in the NavDescription
   *    descPersonality: The personality in the NavDescription
   * @return {cvox.CursorSelection} The new selection.
   */
  go: function(sel, opt_cmdOrDest, desc) {
    if (opt_cmdOrDest instanceof cvox.CursorSelection) {
      var ret = opt_cmdOrDest;
    } else {
      if (ChromeVoxWalkerUnitTestBase.CMD_WHITELIST.indexOf(opt_cmdOrDest) ==
          -1) {
        // Intentionally fail the test if there's a typo.
        throw 'Got an invalid command: "' + opt_cmdOrDest + '".';
      }

      var ret = this.walker[opt_cmdOrDest](sel);
    }

    if (ret == null) {
      assertEquals(null, desc);
      return;
    }
    if (desc == null) {
      assertEquals(null, ret);
      return;
    }

    for (var key in desc) {
      if (ChromeVoxWalkerUnitTestBase.DESC_WHITELIST.indexOf(key) == -1) {
        throw 'Invalid key in desc parameter: "' + key + '".';
      }
    }

    // Intentionally only check one-to-one and not onto. This allows us to
    // write tests that just ignore everything except what we care about.
    if (desc.hasOwnProperty('selText')) {
      assertEquals(desc.selText, ret.start.text);
      assertEquals(desc.selText, ret.end.text);
    }
    if (desc.hasOwnProperty('selNodeId')) {
      assertEquals(desc.selNodeId, ret.start.node.id);
      assertEquals(desc.selNodeId, ret.end.node.id);
    }
    if (desc.hasOwnProperty('selParentNodeId')) {
      assertEquals(desc.selParentNodeId, ret.start.node.parentNode.id);
      assertEquals(desc.selParentNodeId, ret.end.node.parentNode.id);
    }
    if (desc.hasOwnProperty('selStartIndex')) {
      assertEquals(desc.selStartIndex, ret.absStart().index);
    }
    if (desc.hasOwnProperty('selEndIndex')) {
      assertEquals(desc.selEndIndex, ret.absEnd().index);
    }
    if (desc.hasOwnProperty('selReversed')) {
      assertEquals(desc.selReversed, ret.isReversed());
    }

    var trueDesc = this.walker.getDescription(sel, ret)[0];
    if (desc.hasOwnProperty('descText')) {
      assertEquals(desc.descText, trueDesc.text);
    }
    if (desc.hasOwnProperty('descContext')) {
      assertEquals(desc.descContext, trueDesc.context);
    }
    if (desc.hasOwnProperty('descAnnotation')) {
      assertEquals(desc.descAnnotation, trueDesc.annotation);
    }
    if (desc.hasOwnProperty('descUserValue')) {
      assertEquals(desc.descUserValue, trueDesc.userValue);
    }
    if (desc.hasOwnProperty('descPersonality')) {
      assertEquals(desc.descPersonality, trueDesc.personality);
    }

    return ret;
  },
};

/**
 * Whitelist for the commands that are allowed to be executed with go().
 * @type {Array.string}
 * @const
 */
ChromeVoxWalkerUnitTestBase.CMD_WHITELIST =
    ['next', 'sync', 'nextRow', 'nextCol'];

/**
 * Whitelist for the properties that can be asserted with go().
 * @type {Array.string}
 * @const
 */
ChromeVoxWalkerUnitTestBase.DESC_WHITELIST = [
  'selText', 'selNodeId', 'selParentNodeId', 'selStartIndex', 'selEndIndex',
  'selReversed', 'descText', 'descContext', 'descAnnotation', 'descUserValue',
  'descPersonality'
];

/**
 * Adds common walker tests
 * @param {string} testFixture Name of the test fixture class.
 */
ChromeVoxWalkerUnitTestBase.addCommonTests = function(testFixture) {
  /**
   * Ensures that syncing to the beginning and ends of the page return
   * not null.
   */
  TEST_F(testFixture, 'testSyncToPage', function() {
    this.loadDoc(function() { /*!
      <div><p id="a">a</p></div>
    */ });
    var ret = this.walker.begin();
    assertNotEquals(null, ret);
    ret = this.walker.begin({reversed: true});
    assertNotEquals(null, ret);
  });

  /**
   * Ensures that sync(sync(sel)) = sync(sel)
   * TODO (stoarca): The interfaces are not frozen yet. In particular,
   * for TableWalker, sync can return null. Override if it doesn't work yet.
   */
  TEST_F(testFixture, 'testSyncInvariant', function() {
    this.loadDoc(function() { /*!
      <div id="outer">
        <p id="a">a</p>
        <p id="b">b</p>
        <p id="c">c</p>
        <p id="d">d</p>
        <h1 id="A">h1</h1>
        <p id="e">e</p>
        <h1 id="B">h1</h1>
      </div>
    */ });
    var sel = cvox.CursorSelection.fromNode($('outer').firstChild);
    var sync = this.walker.sync(sel);
    var syncsync = this.walker.sync(sync);
    assertEquals(true, sync.equals(syncsync));

    sel = cvox.CursorSelection.fromNode($('a'));
    sync = this.walker.sync(sel);
    syncsync = this.walker.sync(sync);
    assertEquals(true, sync.equals(syncsync));

    sel = cvox.CursorSelection.fromNode($('e'));
    sync = this.walker.sync(sel);
    syncsync = this.walker.sync(sync);
    assertEquals(true, sync.equals(syncsync));

    sel = cvox.CursorSelection.fromBody();
    sync = this.walker.sync(sel);
    syncsync = this.walker.sync(sync);
    assertEquals(true, sync.equals(syncsync));
  });

  /**
   * Ensures that all operations work on an empty body.
   */
  TEST_F(testFixture, 'testEmptyBody', function() {
    var sel = cvox.CursorSelection.fromBody();

    // Testing for infinite loop. If one exists, this test will fail by timing
    // out.
    var sync = this.walker.sync(sel);
    var next = this.walker.next(sel);
  });
};
