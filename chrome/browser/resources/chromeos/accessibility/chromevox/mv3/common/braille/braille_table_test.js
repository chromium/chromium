// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for BrailleTable tests.
 * This is an E2E test because there's no easy way to load a data file in
 * a webui-style test.
 */
ChromeVoxBrailleTableTest = class extends ChromeVoxE2ETest {};

/**
 * Tests that {@code getAll} can fetch and parse the tables file.
 * NOTE: This will need to be adjusted when more tables are added.
 */
TEST_F('ChromeVoxBrailleTableTest', 'testGetAllAndValidate', function() {
  BrailleTable.getAll(this.newCallback(function(tables) {
    assertEquals(184, tables.length);
    assertNotNullNorUndefined(
        BrailleTable.forId(tables, 'en-us-g1'),
        'Can\'t find US English grade 1 table');
    for (let i = 0, table; table = tables[i]; ++i) {
      assertEquals('string', typeof table.id);
      assertTrue(table.dots === '6' || table.dots === '8');

      // Ensure we have an English UI language.
      chrome.i18n.getUILanguage = () => 'en';
      let displayName = BrailleTable.getDisplayName(table);
      assertTrue(
          Boolean(displayName), 'No display name for table: ' + table.id);
      assertTrue(displayName.length > 0);

      // English always uses LibLouis's enDisplayName if possible.
      if (table.enDisplayName) {
        assertTrue(
            displayName.indexOf(table.enDisplayName) >= 0,
            'LibLouis display name no included');
      }

      // Try getting a display name for a non-English language.
      chrome.i18n.getUILanguage = () => 'fr';
      displayName = BrailleTable.getDisplayName(table);
      assertTrue(
          Boolean(displayName), 'No display name for table: ' + table.id);
      assertTrue(displayName.length > 0);

      // Other languages only use the enDisplayName if they need to disambiguate
      // or have no locale name.
      const localeName = chrome.accessibilityPrivate.getDisplayNameForLocale(
          table.locale, table.locale);
      if (!localeName ||
          (table.enDisplayName && table.alwaysUseEnDisplayName)) {
        assertTrue(
            displayName.indexOf(table.enDisplayName) >= 0,
            'No LibLouis display name: ' + displayName +
                ' for non-English locale: ' + table.locale);
      } else {
        assertFalse(
            displayName.indexOf(table.enDisplayName) >= 0,
            displayName + ' should not contain ' + table.enDisplayName);
      }
    }
  }));
});

/** Tests getDisplayName for some specific representative cases. */
TEST_F('ChromeVoxBrailleTableTest', 'testGetDisplayName', function() {
  BrailleTable.getAll(this.newCallback(function(tables) {
    let table = BrailleTable.forId(tables, 'bg');
    assertEquals('Bulgarian, Grade 1', BrailleTable.getDisplayName(table));
    table = BrailleTable.forId(tables, 'ar-ar-g1');
    assertEquals(
        'Arabic (Argentina), Grade 1', BrailleTable.getDisplayName(table));
    table = BrailleTable.forId(tables, 'en-ueb-g1');
    assertEquals(
        'Unified English uncontracted braille, Grade 1',
        BrailleTable.getDisplayName(table));
    table = BrailleTable.forId(tables, 'en-us-g2');
    assertEquals(
        'English (United States), Grade 2', BrailleTable.getDisplayName(table));
  }));
});

/**
 * Tests the getUncontracted function.
 */
TEST_F('ChromeVoxBrailleTableTest', 'testGetUncontracted', function() {
  BrailleTable.getAll(this.newCallback(function(tables) {
    function expectUncontracted(uncontractedId, idToCheck) {
      const checkedTable = BrailleTable.forId(tables, idToCheck);
      const uncontractedTable =
          BrailleTable.getUncontracted(tables, checkedTable);
      assertNotEquals(
          null, uncontractedTable,
          'Table does not have uncontracted table: ' + checkedTable);
      assertEquals(uncontractedId, uncontractedTable.id);
    }
    expectUncontracted('en-nabcc', 'en-us-g2');
    expectUncontracted('en-us-comp8-ext', 'en-us-comp8-ext');
    expectUncontracted('ar-ar-comp8', 'ar-ar-g1');
    expectUncontracted('de-de-comp8', 'de-g2');
  }));
});
