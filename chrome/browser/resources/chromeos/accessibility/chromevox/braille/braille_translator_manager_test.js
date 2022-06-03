// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for BrailleTranslatorManager tests.
 * This is an E2E test because there's no easy way to load a data file in
 * a webui-style test.
 */
ChromeVoxBrailleTranslatorManagerTest = class extends ChromeVoxE2ETest {
  /** @override */
  setUp() {
    this.liblouis = new FakeLibLouis();
    this.manager = new BrailleTranslatorManager(this.liblouis);
    this.liblouis.translatorManager = this.manager;
    // This is called by an event handler in production, but we don't rely
    // on that for this test.
    this.manager.loadLiblouis_();
  }

  addChangeListener(callback) {
    return this.manager.addChangeListener(callOnce(this.newCallback(callback)));
  }
};


/** @extends {LibLouis} */
function FakeLibLouis() {}

FakeLibLouis.prototype = {
  /** @override */
  attachToElement() {},

  /** @override */
  getTranslator(fileNames, callback) {
    const tables = this.translatorManager.getTablesForTest();
    let result = null;
    if (tables != null) {
      const found = tables.filter(function(table) {
        return table.fileNames === fileNames;
      })[0];
      if (found) {
        result = new FakeTranslator(found);
      }
    }
    callback(result);
  }
};

FakeTranslator = class {
  /**
   * @param {BrailleTable.Table} table
   */
  constructor(table) {
    this.table = table;
  }
};

function callOnce(callback) {
  let called = false;
  return function() {
    if (!called) {
      called = true;
      callback.apply(null, arguments);
    }
  };
}

TEST_F('ChromeVoxBrailleTranslatorManagerTest', 'testInitial', function() {
  assertEquals(null, this.manager.getExpandingTranslator());
  assertEquals(null, this.manager.getDefaultTranslator());
  assertEquals(null, this.manager.getUncontractedTranslator());
  this.addChangeListener(function() {
    assertNotEquals(null, this.manager.getExpandingTranslator());
    assertEquals('en-US-comp8', this.manager.getDefaultTranslator().table.id);
    assertEquals(null, this.manager.getUncontractedTranslator());
  });
});

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testRefreshWithoutChange',
    function() {
      this.addChangeListener(function() {
        assertNotEquals(null, this.manager.getExpandingTranslator());
        // This works because the fake liblouis is actually not asynchonous.
        this.manager.addChangeListener(function() {
          assertNotReached('Refresh should not be called without a change.');
        });
        this.manager.refresh(localStorage['brailleTable']);
      });
    });

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testRefreshWithChange',
    function() {
      this.addChangeListener(function() {
        assertNotEquals(null, this.manager.getExpandingTranslator());
        this.addChangeListener(function() {
          assertEquals(
              'en-UEB-g2', this.manager.getDefaultTranslator().table.id);
          assertEquals(
              'en-US-comp8', this.manager.getUncontractedTranslator().table.id);
        });
        this.manager.refresh('en-UEB-g2');
      });
    });
